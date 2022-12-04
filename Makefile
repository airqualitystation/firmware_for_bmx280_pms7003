APPLICATION=airqualitystation

.PHONY: all
all:
	$(info $$MAKEFILE_DEVICE is ${MAKEFILE_DEVICE})
	$(info $$OPERATOR is ${OPERATOR})
	$(info $$DRIVER is ${DRIVER})
	$(info $$REGION is ${REGION})
	$(info $$DS75LX is ${DS75LX})
	$(info $$LM75 is ${LM75})
	$(info $$AT30TES75X is ${AT30TES75X})
	$(info $$GPS is ${GPS})
	$(info $$PMS7003 is ${PMS7003})
	$(info $$BMX280 is ${BMX280})
	
# -----------------------------
# Debug
# -----------------------------

# Set this to 1 to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

#CFLAGS += -DENABLE_DEBUG=1
CFLAGS += -DDEBUG_ASSERT_VERBOSE=1

ifdef MAKEFILE_DEVICE
include $(MAKEFILE_DEVICE)
else
include ./Makefile.device
endif

include ./Makefile.git

# Default region is Europe and default band is 868MHz
REGION ?= EU868

ifndef BOARD
BOARD ?= lora-e5-mini
endif

ifeq ($(BOARD),im880b)
DS75LX ?= 1
GPS ?= 0
# Default radio driver is Semtech SX1272 (used by the IMST iM880ab boards)
DRIVER ?= sx1272
FEATURES_REQUIRED += periph_rtc
endif

ifeq ($(BOARD),nucleo-wl55jc)
GPS ?= 0
DRIVER ?= sx126x_stm32wl
endif

ifeq ($(BOARD),b-l072z-lrwan1)
DS75LX ?= 0
GPS ?= 0
DRIVER ?= sx1276
endif


# IdoSens boards are into git@github.com:CampusIoT/idosens.git

ifeq ($(BOARD),idosens_sensor)
EXTERNAL_BOARD_DIRS ?= ../../../campusiot/idosens/boards
# Builtin sensors
AT30TES75X ?= 1
DRIVER ?= sx1276
endif

ifeq ($(BOARD),idosens_base)
EXTERNAL_BOARD_DIRS ?= ../../../campusiot/idosens/boards
# TODO add ePaper Display driver
DRIVER ?= sx1276
endif

ifeq ($(BOARD),idosens_remote)
EXTERNAL_BOARD_DIRS ?= ../../../campusiot/idosens/boards
DRIVER ?= sx1276
endif

ifeq ($(BOARD),lora-e5-dev)
LM75 ?= 1
GPS ?= 0
DRIVER ?= sx126x_stm32wl
CFLAGS += -DISR_STACKSIZE=2048U
endif

ifeq ($(BOARD),lora-e5-mini)
BOARD = lora-e5-dev
GPS ?= 0
DRIVER ?= sx126x_stm32wl
CFLAGS += -DISR_STACKSIZE=2048U
endif

# Sensors

ifeq ($(DS75LX),1)
USEMODULE += ds75lx
CFLAGS += -DDS75LX=1
endif

ifeq ($(LM75),1)
USEMODULE += lm75
CFLAGS += -DLM75=1
endif

ifeq ($(AT30TES75X),1)
USEMODULE += at30tse75x
CFLAGS += -DAT30TES75X=1
endif

ifeq ($(BMX280),1)
#USEMODULE += periph_i2c
USEMODULE += bme280_i2c
# Set the I2C address for the Bosch Temperature and Humidity BME280 sensor
CFLAGS += -DBMX280=1
CFLAGS += -DBMX280_PARAM_I2C_DEV=I2C_DEV\(0\)
CFLAGS += -DBMX280_PARAM_I2C_ADDR=0x76
endif

ifeq ($(PMS7003),1)
# For PMS7003 Particle Sensor
CFLAGS += -DPMS7003=1
FEATURE_REQUIRED += periph_uart
FEATURES_REQUIRED += periph_gpio
# Reset pin is PA9
CFLAGS += -DPMS7003_RESET_PIN=GPIO_PIN\(0,9\)
endif


ifeq ($(GPS),1)
CFLAGS += -DGPS=1
# define the GNSS module baudrate
CFLAGS += -DSTD_BAUDRATE=$(STD_BAUDRATE)
endif

# TODO Add SAUL for LED

USEMODULE += fmt

USEMODULE += ztimer
USEMODULE += ztimer_sec

USEMODULE += random
USEMODULE += prng_sha1prng

# Watchdog timer values
FEATURES_REQUIRED += periph_wdt

ENABLE_WDT_ZTIMER ?= 1
ifeq ($(ENABLE_WDT_ZTIMER),1)
CFLAGS += -DENABLE_WDT_ZTIMER=1
# 1 for debug the wdt
CFLAGS += -DENABLE_DEBUG_WDT_TIMER=0
endif

# Semtech LoRaMAC

LORA_DRIVER ?= $(DRIVER)
LORA_REGION ?= $(REGION)

USEPKG += semtech-loramac
USEMODULE += auto_init_loramac
USEMODULE += semtech_loramac_rx
USEMODULE += $(LORA_DRIVER)

#
# DRPWSZ_SEQUENCE contains the sequence of triplets <datarate,tx power,payload size>
# If datarate is 255, the ADR is set to TRUE
#

# By default (for all except LLCC68)
ifndef LORAMAC_JOIN_MIN_DATARATE
ifeq ($(DRIVER),llcc68) 
LORAMAC_JOIN_MIN_DATARATE ?= 1
else
LORAMAC_JOIN_MIN_DATARATE ?= 0
endif
endif

# By default
ifndef APP_CLOCK_SYNC
APP_CLOCK_SYNC ?= 1
endif

# By default
ifndef TXPERIOD
TXPERIOD ?= 30
endif

# By default
ifndef TXCNF
TXCNF ?= false
endif

# initial ADR
ADR_ON ?= false

# LORAMAC_CLASS_A, LORAMAC_CLASS_B, LORAMAC_CLASS_C
ENDPOINT_CLASS ?= LORAMAC_CLASS_A
CFLAGS += -DENDPOINT_CLASS=$(ENDPOINT_CLASS)

MIN_PORT ?= 1
MAX_PORT ?= 170


DEVELHELP ?= 1

ifeq ($(OTAA),1)
CFLAGS += -DOTAA=1
else
CFLAGS += -DOTAA=0
endif

ifeq ($(OTAA),0)
CFLAGS += -DDEVADDR=\"$(DEVADDR)\"
CFLAGS += -DAPPSKEY=\"$(APPSKEY)\"
CFLAGS += -DNWKSKEY=\"$(NWKSKEY)\"
endif

ifdef LABEL
CFLAGS += -DLABEL=\"$(LABEL)\"
endif

ifndef SECRET
# SECRET should be changed and kept secret
SECRET ?= cafebabe02000001cafebabe02ffffff
endif

ifndef DEVEUI
USEMODULE += hashes
CFLAGS += -DFORGE_DEVEUI_APPEUI_APPKEY -DSECRET=\"$(SECRET)\"
else
CFLAGS += -DDEVEUI=\"$(DEVEUI)\" -DAPPEUI=\"$(APPEUI)\" -DAPPKEY=\"$(APPKEY)\"
endif

ifndef OPERATOR
OPERATOR ?= Undefined
endif

ifdef FCNT_UP
CFLAGS += -DFCNT_UP=$(FCNT_UP)
endif


CFLAGS += -DREGION_$(REGION)
CFLAGS += -DLORAMAC_REGION_STR=\"$(REGION)\"
#CFLAGS += -DLORAMAC_ACTIVE_REGION=LORAMAC_REGION_$(REGION)
CFLAGS += -DLORAMAC_JOIN_MIN_DATARATE=$(LORAMAC_JOIN_MIN_DATARATE)
CFLAGS += -DDRPWSZ_SEQUENCE=$(DRPWSZ_SEQUENCE)
CFLAGS += -DTXPERIOD_AT_DR0=$(TXPERIOD_AT_DR0)
CFLAGS += -DTXCNF=$(TXCNF)
CFLAGS += -DADR_ON=$(ADR_ON)
CFLAGS += -DDATA_PORT=$(DATA_PORT)

CFLAGS += -DOPERATOR=\"$(OPERATOR)\"

# Send a APP_TIME_REQ every APP_TIME_REQ_PERIOD messages
CFLAGS += -DAPP_CLOCK_SYNC=$(APP_CLOCK_SYNC)
ifndef APP_TIME_REQ_PERIOD
APP_TIME_REQ_PERIOD ?= 50
endif
CFLAGS += -DAPP_TIME_REQ_PERIOD=$(APP_TIME_REQ_PERIOD)

CFLAGS += -DGUARD_SENDER_WAKEUP=1

include $(RIOTBASE)/Makefile.include
