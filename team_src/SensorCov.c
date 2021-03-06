/*
 * SensorCov().c
 *
 *  Created on: Oct 30, 2013
 *      Author: Nathan
 */

#include "all.h"


ops_struct ops_temp;
data_struct data_temp;
stopwatch_struct* BIM_watch;

void SensorCov()
{
	SensorCovInit();
	while (ops.State == STATE_SENSOR_COV)
	{
		LatchStruct();
		SensorCovMeasure();
		UpdateStruct();
		if(data.update == 1)
		{
			FillCANData();
			data.update = 0;
		}
	}
	SensorCovDeInit();
}

void SensorCovInit()
{
	//todo USER: SensorCovInit()
	ops.BIM_State = INIT;
	ops.Flags.bit.BIM_init = 0;
	BQ_Setup();

	EALLOW;
	GpioDataRegs.GPACLEAR.bit.GPIO15 = 1;
	GpioCtrlRegs.GPAMUX1.bit.GPIO15 = 0;         // GPIO
	GpioCtrlRegs.GPADIR.bit.GPIO15 = 1;          // output
	GpioCtrlRegs.GPAQSEL1.bit.GPIO15 = 0;        //Synch to SYSCLKOUT only
	GpioCtrlRegs.GPAPUD.bit.GPIO15 = 1; 		//disable pull up
	GpioDataRegs.GPACLEAR.bit.GPIO15 = 1;
	EDIS;

	EALLOW;
	GpioDataRegs.GPASET.bit.GPIO4 = 1;
	GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 0;         // GPIO
	GpioCtrlRegs.GPADIR.bit.GPIO4 = 1;          // output
	GpioCtrlRegs.GPAQSEL1.bit.GPIO4 = 0;        //Synch to SYSCLKOUT only
	GpioCtrlRegs.GPAPUD.bit.GPIO4 = 1; 		//disable pull up
	GpioDataRegs.GPASET.bit.GPIO4 = 1;
	EDIS;

	EALLOW;
	GpioDataRegs.GPASET.bit.GPIO5 = 1;
	GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 0;         // GPIO
	GpioCtrlRegs.GPADIR.bit.GPIO5 = 1;          // output
	GpioCtrlRegs.GPAQSEL1.bit.GPIO5 = 0;        //Synch to SYSCLKOUT only
	GpioCtrlRegs.GPAPUD.bit.GPIO5 = 1; 		//disable pull up
	GpioDataRegs.GPASET.bit.GPIO5 = 1;
	EDIS;

	BIM_watch = StartStopWatch(100);
	while(isStopWatchComplete(BIM_watch) != 1);		//delay for microsecond for voltage regulator to start up
}


void LatchStruct()
{
	memcpy(&ops_temp, &ops, sizeof(struct OPERATIONS));
	//memcpy(&data_temp, &data, sizeof(struct DATA));		//data is only changed in sensor measure
	ops.Change.all = 0;	//clear change states
}

void SensorCovMeasure()
{

	Check_Bal_Button(); //do correct operation considering balance button

	switch (ops_temp.BIM_State)
	{
	case INIT:
		data_temp.update = 1; //cause an update to clear mailboxes
		BALLEDCLEAR();
		ops_temp.Flags.bit.BIM_init = 0;
		if(NUMBER_OF_BQ_DEVICES != bq_pack_address_discovery())
		{
			ops_temp.BIM_State = INIT_DELAY;
		}
		else
		{
			bq_pack_init();
			ops_temp.BIM_State = COV;
			StopWatchRestart(BIM_watch);
			ops_temp.Flags.bit.BIM_init = 1;
		}
		StopWatchRestartTime(BIM_watch,50000);	// half second delay
		break;
	case INIT_DELAY:
		BALLEDCLEAR();
		if (isStopWatchComplete(BIM_watch) == 1) 	// if delayed enough
		{
			if(NUMBER_OF_BQ_DEVICES != bq_pack_address_discovery()) //try to init again
			{
				ops_temp.BIM_State = INIT_DELAY;					//if didn't work try again
				StopWatchRestart(BIM_watch);
			}
			else
			{
				bq_pack_init();
				ops_temp.BIM_State = COV;
				StopWatchRestartTime(BIM_watch,ops_temp.Update_period);
				ops_temp.Flags.bit.BIM_init = 1;
			}
		}
		break;
	case COV:
		if (isStopWatchComplete(BIM_watch) == 1)					// if delayed conversion
		{
			BMM_Wake();
			bq_pack_start_conv();
			ops_temp.BIM_State = MEASURE;
		}
		break;
	case MEASURE:
		if (READBQDRDY() == 1)										//wait until data is ready
		{
			update_bq_pack_data();									//update data
			CellBalancing();	//balance if ops says so
			Flash_Bal_LED();	//flash led according to balance
			//BMM_Sleep();
			data_temp.update = 1;									//actually latch data
			ops_temp.BIM_State = COV;
			StopWatchRestartTime(BIM_watch,ops_temp.Update_period);
			BIM_LED_Clear();
		}
		break;
	default:
		ops_temp.BIM_State = INIT;
	}
}

void UpdateStruct()
{
	if (data_temp.update == 1)
	{
		memcpy(&data, &data_temp, sizeof(struct DATA));
	}

	//todo USER: UpdateStruct
	//update with node specific op changes

	//if ops is not changed outside of sensor conversion copy temp over, otherwise don't change

	//Change bit is only set by ops changes outside of SensorCov.
	if (ops.Change.bit.State == 0)
	{
		ops.State = ops_temp.State;
	}

	if (ops.Change.bit.Balance == 0)
	{
		ops.Balance = ops_temp.Balance;
	}

	if (ops.Change.bit.Update_Period == 0)
	{
		ops.Update_period = ops_temp.Update_period;
	}

	//don't change BIM State or Flags through CAN

	ops.BIM_State = ops_temp.BIM_State;
	ops.Flags.all = ops_temp.Flags.all;

	ops.Change.all = 0;	//clear change states
}

void SensorCovDeInit()
{
	//todo USER: SensorCovDeInit()
	BIM_low();
	StopStopWatch(BIM_watch);

}
