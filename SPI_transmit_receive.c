#include <SPI_transmit_receive.h>

///////////buffer/////////////////////////////////
unsigned char slaveRxBuffer[SPI_CMD_LENGTH];
unsigned char slaveTxBuffer[SPI_CMD_LENGTH]={0x01,0x02,0x03,0x04,0x05};

///////////buffer/////////////////////////////////
unsigned char slaveRxBuffer_snd[SPI_CMD_LENGTH];
unsigned char slaveTxBuffer_snd[SPI_CMD_LENGTH]={0xaa,0x03,0x00,0x12,0x34};

///////// Callback function for SPI_transfer().////////////
void transferCompleteFxn(SPI_Handle handle, SPI_Transaction *transaction)
{
    sem_post(&slaveSem);
}

////////////////////timeout callback
void CounterTimeoutCb(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask)
{
   timeout_error=true;
   sem_post(&slaveSem);
}

void SPI_params_init()
{
    //////////////setting up waiting for transfer complete semaphore//////////////////////////////
    status = sem_init(&slaveSem, 0, 0);
    if (status != 0) {
         while(1);
    }

    SPI_init();
        /////////////set SPI parameters and open SPI connection///////////////////////////
    SPI_Params_init(&spiParams);
    spiParams.frameFormat = SPI_POL1_PHA1;
    spiParams.mode = SPI_SLAVE;
    spiParams.transferCallbackFxn = transferCompleteFxn;
    spiParams.transferMode = SPI_MODE_CALLBACK;

    /////////Initializing GPT TIMER timeout///////////////////////
    params.width          = GPT_CONFIG_32BIT;
    params.mode           = GPT_MODE_ONESHOT_UP;
    params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_OFF;

    hCountTimer = GPTimerCC26XX_open(Board_GPTIMER0A, &params);
    if(hCountTimer == NULL)
    {
        while(1);
    }
    ///////////Set  Common Timeout value to 20MS ////////////////////
    CounterPeriod = (SysCtrlClockGet()*100UL)/1000UL - 1UL;
    GPTimerCC26XX_setLoadValue(hCountTimer, CounterPeriod);
    /////////////// Register GPTimer interrupt ////////////////
    GPTimerCC26XX_registerInterrupt(hCountTimer, CounterTimeoutCb, GPT_INT_TIMEOUT);

}
uint8_t* SPI_slave_receive()
{
    ////////////launching timeout////////////////////
    CounterCurrTime = GPTimerCC26XX_getValue(hCountTimer);
    GPTimerCC26XX_setLoadValue(hCountTimer, CounterCurrTime + CounterPeriod);
    GPTimerCC26XX_start(hCountTimer);

    SPI_connection_open();
    memset(( void *) slaveRxBuffer, 0, SPI_CMD_LENGTH);
    transaction.count = SPI_CMD_LENGTH;
    transaction.txBuf =(void *) slaveTxBuffer;
    transaction.rxBuf =(void *) slaveRxBuffer;

    ////// SPI transfer;falling to the slaveSPI semaphore, until transaction happened to the end/
    transferOK = SPI_transfer(slaveSpi, &transaction);
    if (transferOK) sem_wait(&slaveSem);

    //////////////STOP TIMeout//////////////////
    GPTimerCC26XX_stop(hCountTimer);

    SPI_connection_close();

   return slaveRxBuffer;
}

void SPI_slave_send(uint16_t* snd_word)
{
    ////////////launching timeout////////////////////
     CounterCurrTime = GPTimerCC26XX_getValue(hCountTimer);
     GPTimerCC26XX_setLoadValue(hCountTimer, CounterCurrTime + CounterPeriod);
     GPTimerCC26XX_start(hCountTimer);

    SPI_connection_open();
    slaveTxBuffer_snd[2]=(unsigned char)(*snd_word>>8);
    slaveTxBuffer_snd[3]=(unsigned char)(0x00FF&*snd_word);
    memset(( void *) slaveRxBuffer_snd, 0, SPI_CMD_LENGTH);
    transaction.count = SPI_CMD_LENGTH;
    transaction.txBuf =(void *) slaveTxBuffer_snd;
    transaction.rxBuf =(void *) slaveRxBuffer_snd;

    ////// SPI transfer;falling to the slaveSPI semaphore, until transaction happened to the end/
    transferOK = SPI_transfer(slaveSpi, &transaction);
    if (transferOK) sem_wait(&slaveSem);

    //////////////STOP TIMER//////////////////
    GPTimerCC26XX_stop(hCountTimer);
    SPI_connection_close();

}

void SPI_connection_open()
{
    slaveSpi = SPI_open(0, &spiParams);
}

void SPI_connection_close()
{
    SPI_close(slaveSpi);
}

