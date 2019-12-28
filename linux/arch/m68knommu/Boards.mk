# Put definitions for platforms and boards in here.

ifdef CONFIG_M68328
PLATFORM := 68328
ifdef CONFIG_PILOT
BOARD := pilot
endif
endif

ifdef CONFIG_M68EZ328
PLATFORM := 68EZ328
ifdef CONFIG_PILOT
BOARD := PalmV
endif
ifdef CONFIG_UCSIMM
BOARD := ucsimm
endif
ifdef CONFIG_ALMA_ANS
BOARD := alma_ans
endif
ifdef CONFIG_M68EZ328ADS
BOARD := ads
endif
endif

ifdef CONFIG_M68332
PLATFORM := 68332
endif

ifdef CONFIG_M68EN302
PLATFORM := 68EN302
endif

ifdef CONFIG_M68360
PLATFORM := 68360
endif

ifdef CONFIG_M5204
PLATFORM := 5204
BOARD := SBC5204
endif

ifdef CONFIG_M5206
PLATFORM := 5206
BOARD := ARNEWSH
endif

ifdef CONFIG_M5206e
PLATFORM := 5206e
ifdef CONFIG_CADRE3
BOARD := CADRE3
endif
ifdef CONFIG_ELITE
BOARD := eLITE
endif
ifdef CONFIG_NETtel
BOARD := NETtel
endif
ifdef CONFIG_TELOS
BOARD := toolvox
endif
ifdef CONFIG_CFV240
BOARD := CFV240
endif
endif

ifdef CONFIG_M5307
PLATFORM := 5307
ifdef CONFIG_ARNEWSH
BOARD := ARNEWSH
endif
ifdef CONFIG_NETtel
BOARD := NETtel
endif
ifdef CONFIG_eLIA
BOARD := eLIA
endif
ifdef CONFIG_DISKtel
BOARD := DISKtel
endif
ifdef CONFIG_CADRE3
BOARD := CADRE3
endif
endif

ifdef CONFIG_M5407
PLATFORM := 5407
BOARD := MOTOROLA
endif

export BOARD
export PLATFORM

