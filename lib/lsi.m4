divert(-1)
#
#                             COPYRIGHT
# 
#   PCB, interactive printed circuit board design
#   Copyright (C) 1994,1995,1996 Thomas Nau
# 
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
# 
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
# 
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# 
#   Contact addresses for paper mail and Email:
#   Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
#   Thomas.Nau@rz.uni-ulm.de
# 
#   RCS: $Id$
#
# ----------------------------------------------------------------------
# several different microcontrollers from the PIC family
# donated by ?!?, thanks anyway
#
define(`Description_BT484_plcc', `color lookup table')
define(`Param1_BT484_plcc', 84)
define(`Param2_BT484_plcc', 150)
define(`PinList_BT484_plcc', ``PA0',`PA1',`PA2',`PA3',`PA4',`PA5',`PA6',`PA7',`PB0',`PB1',`PB2',`PB3',`PB4',`PB5',`PB6',`PB7',`PC0',`PC1',`PC2',`PC3',`PC4',`PC5',`PC6',`PC7',`PD0',`PD1',`PD2',`PD3',`PD4',`PD5',`PD6',`PD7',`Adjust',`Gnd',`Red',`Gnd',`Green',`Gnd',`Blue',`Vaa',`Comp',`Vaa',`VRefIn',`VRefOut',`Vaa',`/Sense',`/Reset',`/Wr',`/Rd',`RS0',`RS1',`RS2',`RS3',`D0',`D1',`D2',`D3',`D4',`D5',`D6',`D7',`/OddEven',`CDE',`/CSync',`/CBlank',`PortSel',`V0',`V1',`V2',`V3',`V4',`V5',`V6',`V7',`Vaa',`PClk1',`Vaa',`PClk0',`Vaa',`Gnd',`LClk',`Gnd',`SClk',`Gnd'')

define(`Description_I82077_plcc', `floppy controller')
define(`Param1_I82077_plcc', 68)
define(`Param2_I82077_plcc', 150)
define(`PinList_I82077_plcc', ``WP',`Trk0',`/Dack',`/RD',`/WR',`/CS',`A0',`A1',`Gnd',`A2',`D0',`Gnd',`D1',`D2',`D3',`Gnd',`D4',`Vcc',`D5',`D6',`Gnd',`D7',`Int',`Drq',`TC',`INDX',`IDENT',`DRate0',`DRate1',`DRV2',`DskChg',`Reset',`X1',`X2',`/Invert',`Gnd',`LoFil',`HiFil',`PLL0',`Vcc',`RdData',`NC',`NC',`NC',`AGnd',`AVcc',`NC',`MFM',`DenSel',`Gnd',`HDSel',`We',`WrData',`Gnd',`Step',`Dir',`ME0',`DS0',`Gnd',`Vcc',`ME1',`DS1',`ME2',`DS2',`Gnd',`ME3',`DS3',`Vcc'')

define(`Description_MC68000_dil', `CPU')
define(`Param1_MC68000_dil', 64)
define(`Param2_MC68000_dil', 900)
define(`PinList_MC68000_dil', ``D4',`D3',`D2',`D1',`D0',`/As',`/Uds',`/Lds',`R/W',`/Dtack',`/Bg',`/Bgack',`/Br',`Vcc',`Clk',`Gnd',`/Halt',`/Reset',`/Vma',`E',`/Vpa',`/Berr',`/Ipl2',`/Ipl1',`/Ipl0',`Fc2',`Fc1',`Fc0',`A1',`A2',`A3',`A4',`A5',`A6',`A7',`A8',`A9',`A10',`A11',`A12',`A13',`A14',`A15',`A16',`A17',`A18',`A19',`A20',`Vcc',`A21',`A22',`A23',`Gnd',`D15',`D14',`D13',`D12',`D11',`D10',`D9',`D8',`D7',`D6',`D5'')

define(`Description_MC68008_plcc', `CPU')
define(`Param1_MC68008_plcc', 52)
define(`Param2_MC68008_plcc', 150)
define(`PinList_MC68008_plcc', ``A2',`A3',`A4',`A5',`A6',`A7',`A8',`A9',`A10',`A11',`A12',`A13',`A21',`A14',`Vcc',`A15',`Gnd',`A16',`A17',`A18',`A19',`A20',`D7',`D6',`D5',`D4',`D3',`D2',`D1',`D0',`/As',`/Ds',`R/W',`/Dtack',`/Bg',`/BgAck',`/Br',`Clk',`Gnd',`/Halt',`/Reset',`E',`/Vpa',`/Berr',`/Ipl1',`/Ipl2',`/Ipl0',`Fc2',`Fc1',`Fc0',`A0',`A1'')

define(`Description_MC68681_dil', `DUART')
define(`Param1_MC68681_dil', 40)
define(`Param2_MC68681_dil', 600)
define(`PinList_MC68681_dil', ``Rs1',`Ip3',`Rs2',`Ip1',`Rs3',`Rs4',`Ip0',`R/W',`/Dtack',`RxDB',`TxDB',`Op1',`Op3',`Op5',`Op7',`D1',`D3',`D5',`D7',`Gnd',`/Irq',`D6',`D4',`D2',`D0',`Op6',`Op4',`Op2',`Op0',`TxDA',`RxDA',`X1/Clk',`X2',`/Reset',`/Cs',`Ip2',`/Iack',`Ip5',`Ip4',`Vcc'')

define(`Description_MC68681_plcc', `DUART')
define(`Param1_MC68681_plcc', 44)
define(`Param2_MC68681_plcc', 150)
define(`PinList_MC68681_plcc', ``NC',`RS1',`IP3',`RS2',`IP1',`RS3',`RS4',`IP0',`R-/W',`/DTAck',`RxD1',`NC',`TxD1',`OP1',`OP3',`OP5',`OP7',`D1',`D3',`D5',`D7',`Gnd',`NC',`/Irq',`D6',`D4',`D2',`D0',`OP6',`OP4',`OP2',`OP0',`TxD0',`NC',`RxD0',`X1-Clk',`X2',`/Reset',`/CS',`IP2',`/IAck',`IP5',`IP4',`Vcc'')

define(`Description_PEB2086N_plcc', `ISAC-S')
define(`Param1_PEB2086N_plcc', 44)
define(`Param2_PEB2086N_plcc', 150)
define(`PinList_PEB2086N_plcc', ``AD4',`AD5',`AD6',`AD7',`SDAR',`A1',`SDAX/SDS1',`SCA/SDS2',`RST',`A5',`Vssd',`DCL',`FSC1',`FSC2',`M1',`X2',`A4',`A3',`NC',`X1',`M0',`CP/BCL',`/INT',`Vssa',`XTAL2',`XTAL1',`SR2',`SR1',`NC',`NC',`Vdd',`SX1',`SX2',`IDP0',`IDP1',`ALE',`/CS',`/WR',`/RD',`A0',`AD0',`AD1',`AD2',`AD3'')

define(`Description_MC68HC11_plcc', `micro controller')
define(`Param1_MC68HC11_plcc', 52)
define(`Param2_MC68HC11_plcc', 150)
define(`PinList_MC68HC11_plcc', ``Vss',`MdB_StB',`MdA_/LIR',`StA_/AS',`E',`StB_R/W',`EXTAL',`XTAL',`PC0_AD0',`PC1_AD1',`PC2_AD2',`PC3_AD3',`PC4_AD4',`PC5_AD5',`PC6_AD6',`PC7_AD7',`/RESET',`/XIRQ',`/IRQ',`PD0_RxD',`PD1_Txd',`PD2_MISO',`PD3_MOSI',`PD4_SCK',`PD5_/SS',`Vdd',`PA7_OCAI',`PA6_OC2',`PA5_OC3',`PA4_OC4',`PA3_OCIC',`PA2_IC1',`PA1_IC2',`PA0_IC3',`PB7_A15',`PB6_A14',`PB5_A13',`PB4_A12',`PB3_A11',`PB2_A10',`PB1_A9',`PB0_A8',`PE0_AN0',`PE4_AN4',`PE1_AN1',`PE5_AN5',`PE2_AN2',`PE6_AN6',`PE3_AN3',`PE7_AN7',`VRef_l',`VRef_h'')

define(`Description_PIC16C54_dil', `micro controller')
define(`Param1_PIC16C54_dil', 18)
define(`Param2_PIC16C54_dil', 300)
define(`PinList_PIC16C54_dil', ``RA2',`RA3',`RTCC',`/MCLR',`VSS',`RB0',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7',`VDD',`OSC2',`OSC1',`RA0',`RA1'')

define(`Description_PIC16C55_dil', `micro controller')
define(`Param1_PIC16C55_dil', 28)
define(`Param2_PIC16C55_dil', 600)
define(`PinList_PIC16C55_dil', ``RTCC',`VDD',`nc',`VSS',`nc',`RA0',`RA1',`RA2',`RA3',`RB0',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7',`RC0',`RC1',`RC2',`RC3',`RC4',`RC5',`RC6',`RC7',`OSC2',`OSC1',`/MCLR'')

define(`Description_PIC16C61_dil', `micro controller')
define(`Param1_PIC16C61_dil', 18)
define(`Param2_PIC16C61_dil', 300)
define(`PinList_PIC16C61_dil', ``RA2',`RA3',`RA4',`/MCLR',`VSS',`RB0/INT',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7',`VDD',`OSC2',`OSC1',`RA0',`RA1'')

define(`Description_PIC16C64_dil', `micro controller')
define(`Param1_PIC16C64_dil', 40)
define(`Param2_PIC16C64_dil', 600)
define(`PinList_PIC16C64_dil', ``/MCLR',`RA0',`RA1',`RA2',`RA3',`RA4/T0CKI',`RA5/SS',`RE0/RD',`RE1/WR',`RE2/CS',`VDD',`VSS',`OSC1',`OSC2',`RC0/T0OSO/T1CKI',`RC1/T0OSI',`RC2/CCPI',`RC3/SCK/SCL',`RD0/PSP0',`RD1/PSP1',`RD2/PSP2',`RD3/PSP3',`RC4/SDI/SDA',`RC5/SDO',`RC6',`RC7',`RD4/PSP4',`RD5/PSP5',`RD6/PSP6',`RD7/PSP7',`VSS',`VDD',`RB0/INT',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7'')

define(`Description_PIC16C71_dil', `micro controller')
define(`Param1_PIC16C71_dil', 18)
define(`Param2_PIC16C71_dil', 300)
define(`PinList_PIC16C71_dil', ``RA2/AIN2',`RA3/AIN3',`RA4/T0CKI',`/MCLR',`VSS',`RB0/INT',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7',`VDD',`OSC2',`OSC1',`RA0/AIN0',`RA1/AIN1'')

define(`Description_PIC16C74_dil', `micro controller')
define(`Param1_PIC16C74_dil', 40)
define(`Param2_PIC16C74_dil', 600)
define(`PinList_PIC16C74_dil', ``/MCLR',`RA0/AN0',`RA1/AN1',`RA2/AN2',`RA3/AN3',`RA4/T0CKI',`RA5/AN4/SS',`RE0/RD/AN5',`RE1/WR/AN6',`RE2/CS/AN7',`VDD',`VSS',`OSC1',`OSC2',`RC0/T1OSO/T1CKI',`RC1/T0OSI/CCP2',`RC2/CCPI',`RC3/SCK/SCL',`RD0/PSP0',`RD1/PSP1',`RD2/PSP2',`RD3/PSP3',`RC4/SDI/SDA',`RC5/SDO',`RC6/TX/CK',`RC7/RX/DT',`RD4/PSP4',`RD5/PSP5',`RD6/PSP6',`RD7/PSP7',`VSS',`VDD',`RB0/INT',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7'')

define(`Description_PIC16C84_dil', `micro controller')
define(`Param1_PIC16C84_dil', 18)
define(`Param2_PIC16C84_dil', 300)
define(`PinList_PIC16C84_dil', ``RA2',`RA3',`RA4/T0CKI',`/MCLR',`VSS',`RB0/INT',`RB1',`RB2',`RB3',`RB4',`RB5',`RB6',`RB7',`VDD',`OSC2',`OSC1',`RA0',`RA1'')

define(`Description_PIC17C42_dil', `micro controller')
define(`Param1_PIC17C42_dil', 40)
define(`Param2_PIC17C42_dil', 600)
define(`PinList_PIC17C42_dil', ``VDD',`RC0/AD0',`RC1/AD1',`RC2/AD2',`RC3/AD3',`RC4/AD4',`RC5/AD5',`RC6/AD6',`RC7/AD7',`VSS',`RB0/CAP1',`RB1/CAP2',`RB2/PWM1',`RB3/PWM2',`RB4/TCLK12',`RB5/TCLK3',`RB6',`RB7',`OSC1',`OSC2',`RA5/TX/CK',`RA4/RX/DT',`RA3',`RA2',`RA1/T0CKI',`RA0/INT',`TEST',`RE2/WR',`RE1/OE',`RE0/ALE',`VSS',`/MCLR',`RD7/AD15',`RD6/AD14',`RD5/AD13',`RD4/AD12',`RD3/AD11',`RD2/AD10',`RD1/AD9',`RD0/AD8'')

define(`Description_RTC62421_dil', `real-time clock')
define(`Param1_RTC62421_dil', 18)
define(`Param2_RTC62421_dil', 300)
define(`PinList_RTC62421_dil', ``Std.P',`/Cs0',`Ale',`A0',`A1',`A2',`A3',`/Read',`Gnd',`/Write',`D3',`D2',`D1',`D0',`Cs1',`(Vdd)',`(Vdd)',`Vdd'')

define(`Description_TI34010_plcc', `graphic processor')
define(`Param1_TI34010_plcc', 68)
define(`Param2_TI34010_plcc', 150)
define(`PinList_TI34010_plcc', ``Gnd',`Run/Emu',`/Reset',`VClk',`InClk',`/LInt1',`/LInt2',`/Hold',`LRdy',`LAD0',`LAD1',`LAD2',`LAD3',`LAD4',`LAD5',`LAD6',`LAD7',`Gnd',`LAD8',`LAD9',`LAD10',`LAD11',`LAD12',`LAD13',`LAD14',`LAD15',`Vcc',`LClk1',`LClk2',`/HSync',`/VSync',`/Blank',`/Hlda/Emu',`/LAL',`Gnd',`DDout',`/DEn',`/RAS',`/CAS',`/We',`/TRQE',`/HInt',`HRdy',`HD15',`HD14',`HD13',`HD12',`HD11',`HD10',`HD9',`HD8',`Gnd',`HD7',`HD6',`HD5',`HD4',`HD3',`HD2',`HD1',`HD0',`Vcc',`/HUDS',`/HLDS',`/HRead',`/HWrite',`/HCs',`HFS0',`HFS1'')

define(`Description_WD33C93A_dil', `SE-SCSI-I controller')
define(`Param1_WD33C93A_dil', 40)
define(`Param2_WD33C93A_dil', 600)
define(`PinList_WD33C93A_dil', ``I/O',`/MSG',`GND',`C/D',`/BSY',`/SEL',`CLK',`/DRQ',`/DACK',`INTRQ',`D0',`D1',`D2',`D3',`D4',`D5',`D6',`D7',`A0',`GND',`/CS',`/WE',`/RE',`ALE',`/SDP',`/SD0',`/SD1',`GND',`/SD2',`/SD3',`/SD4',`/SD5',`/SD6',`/SD7',`GND',`/RESET',`/ATN',`/ACK',`/REQ',`Vcc'')

# ----------------------------------------------------------------------
# use a special definition for some PGA packaged circuits
#
define(`Description_MC68030_pga', `CPU')
define(`PKG_MC68030_pga',
	`Element(0x00 "$1" "$2" "$3" 450 650 0 100 0x00)
	(
	Pin(50 50 50 20 "/BR" 0x201)
	Pin(150 50 50 20 "A0" 0x01)
	Pin(250 50 50 20 "A30" 0x01)
	Pin(350 50 50 20 "A28" 0x01)
	Pin(450 50 50 20 "A26" 0x01)
	Pin(550 50 50 20 "A24" 0x01)
	Pin(650 50 50 20 "A23" 0x01)
	Pin(750 50 50 20 "A21" 0x01)
	Pin(850 50 50 20 "A19" 0x01)
	Pin(950 50 50 20 "A17" 0x01)
	Pin(1050 50 50 20 "A15" 0x01)
	Pin(1150 50 50 20 "A13" 0x01)
	Pin(1250 50 50 20 "A10" 0x01)
	Pin(50 150 50 20 "/RMC" 0x01)
	Pin(150 150 50 20 "/BG" 0x01)
	Pin(250 150 50 20 "A31" 0x01)
	Pin(350 150 50 20 "A29" 0x01)
	Pin(450 150 50 20 "A27" 0x01)
	Pin(550 150 50 20 "A25" 0x01)
	Pin(650 150 50 20 "A22" 0x01)
	Pin(750 150 50 20 "A20" 0x01)
	Pin(850 150 50 20 "A16" 0x01)
	Pin(950 150 50 20 "A14" 0x01)
	Pin(1050 150 50 20 "A12" 0x01)
	Pin(1150 150 50 20 "A8" 0x01)
	Pin(1250 150 50 20 "A7" 0x01)
	Pin(50 250 50 20 "FC1" 0x01)
	Pin(150 250 50 20 "/CIOUT" 0x01)
	Pin(250 250 50 20 "/BGACK" 0x01)
	Pin(350 250 50 20 "A1" 0x01)
	Pin(450 250 50 20 "GND" 0x01)
	Pin(550 250 50 20 "VCC" 0x01)
	Pin(650 250 50 20 "GND" 0x01)
	Pin(750 250 50 20 "A18" 0x01)
	Pin(850 250 50 20 "GND" 0x01)
	Pin(950 250 50 20 "A11" 0x01)
	Pin(1050 250 50 20 "A9" 0x01)
	Pin(1150 250 50 20 "A5" 0x01)
	Pin(1250 250 50 20 "A4" 0x01)
	Pin(50 350 50 20 "FC2" 0x01)
	Pin(150 350 50 20 "FC0" 0x01)
	Pin(250 350 50 20 "/OCS" 0x01)
	Pin(350 350 50 20 "VCC" 0x01)
	Pin(450 350 50 20 "NC" 0x01)
	Pin(950 350 50 20 "VCC" 0x01)
	Pin(1050 350 50 20 "A6" 0x01)
	Pin(1150 350 50 20 "A3" 0x01)
	Pin(1250 350 50 20 "A2" 0x01)
	Pin(50 450 50 20 "CLK" 0x01)
	Pin(150 450 50 20 "/AVEC" 0x01)
	Pin(250 450 50 20 "GND" 0x01)
	Pin(1050 450 50 20 "GND" 0x01)
	Pin(1150 450 50 20 "NC" 0x01)
	Pin(1250 450 50 20 "/IPEND" 0x01)
	Pin(50 550 50 20 "/DSACK0" 0x01)
	Pin(150 550 50 20 "VCC" 0x01)
	Pin(250 550 50 20 "GND" 0x01)
	Pin(350 550 50 20 "NC" 0x01)
	Pin(950 550 50 20 "NC" 0x01)
	Pin(1050 550 50 20 "VCC" 0x01)
	Pin(1150 550 50 20 "/RESET" 0x01)
	Pin(1250 550 50 20 "/MMUDIS" 0x01)
	Pin(50 650 50 20 "/STERM" 0x01)
	Pin(150 650 50 20 "/DSACK1" 0x01)
	Pin(250 650 50 20 "GND" 0x01)
	Pin(1050 650 50 20 "GND" 0x01)
	Pin(1150 650 50 20 "/IPL2" 0x01)
	Pin(1250 650 50 20 "/IPL1" 0x01)
	Pin(50 750 50 20 "/BERR" 0x01)
	Pin(150 750 50 20 "/HALT" 0x01)
	Pin(250 750 50 20 "VCC" 0x01)
	Pin(1050 750 50 20 "VCC" 0x01)
	Pin(1150 750 50 20 "/CDIS" 0x01)
	Pin(1250 750 50 20 "/IPL0" 0x01)
	Pin(50 850 50 20 "/CBACK" 0x01)
	Pin(150 850 50 20 "/AS" 0x01)
	Pin(250 850 50 20 "GND" 0x01)
	Pin(1050 850 50 20 "GND" 0x01)
	Pin(1150 850 50 20 "/STATUS" 0x01)
	Pin(1250 850 50 20 "/REFILL" 0x01)
	Pin(50 950 50 20 "/CBREQ" 0x01)
	Pin(150 950 50 20 "/DS" 0x01)
	Pin(250 950 50 20 "SIZ1" 0x01)
	Pin(350 950 50 20 "VCC" 0x01)
	Pin(450 950 50 20 "NC" 0x01)
	Pin(950 950 50 20 "VCC" 0x01)
	Pin(1050 950 50 20 "D5" 0x01)
	Pin(1150 950 50 20 "D1" 0x01)
	Pin(1250 950 50 20 "D0" 0x01)
	Pin(50 1050 50 20 "/CIIN" 0x01)
	Pin(150 1050 50 20 "SIZ0" 0x01)
	Pin(250 1050 50 20 "R/W" 0x01)
	Pin(350 1050 50 20 "D30" 0x01)
	Pin(450 1050 50 20 "GND" 0x01)
	Pin(550 1050 50 20 "VCC" 0x01)
	Pin(650 1050 50 20 "GND" 0x01)
	Pin(750 1050 50 20 "GND" 0x01)
	Pin(850 1050 50 20 "GND" 0x01)
	Pin(950 1050 50 20 "D10" 0x01)
	Pin(1050 1050 50 20 "D7" 0x01)
	Pin(1150 1050 50 20 "D4" 0x01)
	Pin(1250 1050 50 20 "D2" 0x01)
	Pin(50 1150 50 20 "/DBEN" 0x01)
	Pin(150 1150 50 20 "/ECS" 0x01)
	Pin(250 1150 50 20 "D29" 0x01)
	Pin(350 1150 50 20 "D27" 0x01)
	Pin(450 1150 50 20 "D24" 0x01)
	Pin(550 1150 50 20 "D22" 0x01)
	Pin(650 1150 50 20 "D20" 0x01)
	Pin(750 1150 50 20 "D17" 0x01)
	Pin(850 1150 50 20 "D14" 0x01)
	Pin(950 1150 50 20 "D12" 0x01)
	Pin(1050 1150 50 20 "D9" 0x01)
	Pin(1150 1150 50 20 "D6" 0x01)
	Pin(1250 1150 50 20 "D3" 0x01)
	Pin(50 1250 50 20 "D31" 0x01)
	Pin(150 1250 50 20 "D28" 0x01)
	Pin(250 1250 50 20 "D26" 0x01)
	Pin(350 1250 50 20 "D25" 0x01)
	Pin(450 1250 50 20 "D23" 0x01)
	Pin(550 1250 50 20 "D21" 0x01)
	Pin(650 1250 50 20 "D19" 0x01)
	Pin(750 1250 50 20 "D18" 0x01)
	Pin(850 1250 50 20 "D16" 0x01)
	Pin(950 1250 50 20 "D15" 0x01)
	Pin(1050 1250 50 20 "D13" 0x01)
	Pin(1150 1250 50 20 "D11" 0x01)
	Pin(1250 1250 50 20 "D8" 0x01)
	ElementLine(30 0 1300 0 20)
	ElementLine(1300 0 1300 1300 20)
	ElementLine(1300 1300 0 1300 20)
	ElementLine(0 1300 0 30 20)
	ElementLine(0 30 30 0 20)
	ElementLine(0 100 100 100 10)
	ElementLine(100 100 100 0 10)
	Mark(50 50)
	)
')

define(`Description_MC68881_pga', `FPU')
define(`PKG_MC68881_pga',
	`Element(0x00 "$1" "$2" "$3" 350 450 0 100 0x00)
	(
	Pin(50 50 50 20 "VCC" 0x201)
	Pin(150 50 50 20 "GND" 0x01)
	Pin(250 50 50 20 "D0" 0x01)
	Pin(350 50 50 20 "D1" 0x01)
	Pin(450 50 50 20 "D3" 0x01)
	Pin(550 50 50 20 "D4" 0x01)
	Pin(650 50 50 20 "D6" 0x01)
	Pin(750 50 50 20 "D7" 0x01)
	Pin(850 50 50 20 "D8" 0x01)
	Pin(950 50 50 20 "GND" 0x01)
	Pin(50 150 50 20 "VCC" 0x01)
	Pin(150 150 50 20 "GND" 0x01)
	Pin(250 150 50 20 "GND" 0x01)
	Pin(350 150 50 20 "/SENSE" 0x01)
	Pin(450 150 50 20 "D2" 0x01)
	Pin(550 150 50 20 "D5" 0x01)
	Pin(650 150 50 20 "GND" 0x01)
	Pin(750 150 50 20 "VCC" 0x01)
	Pin(850 150 50 20 "D10" 0x01)
	Pin(950 150 50 20 "D11" 0x01)
	Pin(50 250 50 20 "GND" 0x01)
	Pin(150 250 50 20 "CLK" 0x01)
	Pin(250 250 50 20 "GND" 0x01)
	Pin(750 250 50 20 "D9" 0x01)
	Pin(850 250 50 20 "D13" 0x01)
	Pin(950 250 50 20 "D14" 0x01)
	Pin(50 350 50 20 "/RESET" 0x01)
	Pin(150 350 50 20 "GND" 0x01)
	Pin(850 350 50 20 "D12" 0x01)
	Pin(950 350 50 20 "D15" 0x01)
	Pin(50 450 50 20 "NC" 0x01)
	Pin(150 450 50 20 "VCC" 0x01)
	Pin(850 450 50 20 "VCC" 0x01)
	Pin(950 450 50 20 "GND" 0x01)
	Pin(50 550 50 20 "/SIZE" 0x01)
	Pin(150 550 50 20 "GND" 0x01)
	Pin(850 550 50 20 "D17" 0x01)
	Pin(950 550 50 20 "D16" 0x01)
	Pin(50 650 50 20 "/DS" 0x01)
	Pin(150 650 50 20 "A4" 0x01)
	Pin(850 650 50 20 "D20" 0x01)
	Pin(950 650 50 20 "D18" 0x01)
	Pin(50 750 50 20 "/AS" 0x01)
	Pin(150 750 50 20 "A2" 0x01)
	Pin(250 750 50 20 "A0" 0x01)
	Pin(750 750 50 20 "VCC" 0x01)
	Pin(850 750 50 20 "GND" 0x01)
	Pin(950 750 50 20 "D19" 0x01)
	Pin(50 850 50 20 "A3" 0x01)
	Pin(150 850 50 20 "VCC" 0x01)
	Pin(250 850 50 20 "/CS" 0x01)
	Pin(350 850 50 20 "/DSACK0" 0x01)
	Pin(450 850 50 20 "D31" 0x01)
	Pin(550 850 50 20 "D28" 0x01)
	Pin(650 850 50 20 "D25" 0x01)
	Pin(750 850 50 20 "GND" 0x01)
	Pin(850 850 50 20 "D23" 0x01)
	Pin(950 850 50 20 "D21" 0x01)
	Pin(50 950 50 20 "A1" 0x01)
	Pin(150 950 50 20 "R/W" 0x01)
	Pin(250 950 50 20 "GND" 0x01)
	Pin(350 950 50 20 "/DSACK1" 0x01)
	Pin(450 950 50 20 "D30" 0x01)
	Pin(550 950 50 20 "D29" 0x01)
	Pin(650 950 50 20 "D27" 0x01)
	Pin(750 950 50 20 "D26" 0x01)
	Pin(850 950 50 20 "D24" 0x01)
	Pin(950 950 50 20 "D22" 0x01)
	ElementLine(30 0 1000 0 20)
	ElementLine(1000 0 1000 1000 20)
	ElementLine(1000 1000 0 1000 20)
	ElementLine(0 1000 0 30 20)
	ElementLine(0 30 30 0 20)
	ElementLine(0 100 100 100 10)
	ElementLine(100 100 100 0 10)
	Mark(50 50)
	)
')

# ------------------------------------------------------------------------
# based on mail by Volker Bosch (bosch@iema.e-technik.uni-stuttgart.de)
#
define(`Description_MC68332_qfp', `micro controller')
define(`Param1_MC68332_qfp', 132)
define(`PinList_MC68332_qfp', ``Vdd',`Vss(g)',`TpuCh11',`TpuCh10',`TpuCh9',`TpuCh8',`Vdd',`Vss(g)',`TpuCh7',`TpuCh6',`TpuCh5',`TpuCh4',`TpuCh3',`TpuCh2',`TpuCh1',`TpuCh0',`Vss(g)',`Vdd',`Vstby',`A1',`A2',`A3',`A4',`A5',`A6',`A7',`A8',`Vdd',`Vss(g)',`A9',`A10',`A11',`A12',`Vss(g)',`A13',`A14',`A15',`A16',`Vdd',`Vss(g)',`A17',`A18',`MISO',`MOSI',`SCk',`/SS',`PCS1',`PCS2',`PCS3',`Vdd',`Vss(g)',`TxD',`RxD',`DSO',`DSI',`DSClk',`TSC',`Freeze',`Vss(g)',`Xtal',`VddSyn',`EXtal',`Vdd',`XFc',`Vdd',`ClkOut',`Vss(g)',`/Res',`/Halt',`/BErr',`PF7',`PF6',`PF5',`PF4',`PF3',`PF2',`PF1',`PF0',`R/W',`PE7',`PE6',`PE5',`Vss(g)',`Vdd',`PE4',`PE3',`PE2',`PE1',`PE0',`A0',`D15',`D14',`D13',`D12',`Vss(g)',`Vdd',`D11',`D10',`D9',`D8',`Vss(g)',`D7',`D6',`D5',`D4',`Vss(g)',`Vdd',`D3',`D2',`D1',`D0',`/CSboot',`/CS0',`/CS1',`/CS2',`Vdd',`Vss(g)',`/CS3',`/CS4',`/CS5',`/CS6',`/CS7',`/CS8',`/CS9',`/CS10',`Vdd',`Vss(g)',`T2Clk',`TpuCh15',`TpuCh14',`TpuCh13',`TpuCh12'')

# ------------------------------------------------------------------------
# based on mail by Olaf Kaluza (olaf@criseis.ruhr.de)
#
define(`Description_MAB8031AH_dil', `micro controller')
define(`Param1_MAB8031AH_dil', 40)
define(`Param2_MAB8031AH_dil', 600)
define(`PinList_MAB8031AH_dil', ``P1.0',`P1.1',`P1.2',`P1.3',`P1.4',`P1.5',`P1.6',`P1.7',`RST',`RxD/P3.0',`TxD/P3.1',`/INT0/P3.2',`/INT1/P3.3',`T0/P3.4',`T1/P3.5',`/WR/P3.6',`/RD/P3.7',`Xtal2',`Xtal1',`Uss',`P2.0/A8',`P2.1/A9',`P2.2/A10',`P2.3/A11',`P2.4/A12',`P2.5/A13',`P2.6/A14',`P2.7/A15',`/PSEN',`ALE',`/EA',`AD7',`AD6',`AD5',`AD4',`AD3',`AD2',`AD1',`AD0',`Ucc'')

define(`Description_Z8536_dil', `CIO counter/timer with parallel I/O unit')
define(`Param1_Z8536_dil', 40)
define(`Param2_Z8536_dil', 600)
define(`PinList_Z8536_dil', ``D4',`D5',`D6',`D7',`/RD',`/WR',`Gnd',`PB0',`PB1',`PB2',`PB3',`PB4',`PB5',`PB6',`PB7',`PCLK',`IEI',`IEO',`PC0',`PC1',`PC2',`PC3',`/Vcc',`/INT',`/INTACK',`PA7',`PA6',`PA5',`PA4',`PA3',`PA2',`PA1',`PA0',`A0',`A1',`/CE',`D0',`D1',`D2',`D3'')

define(`Description_6551_dil', `ACIA for 65xx series')
define(`Param1_6551_dil', 28)
define(`Param2_6551_dil', 600)
define(`PinList_6551_dil', ``Vss',`CS0',`/CS1',`/Reset',`RxC',`XTLI',`XTLO',`/RTS',`/CTS',`TxD',`/DTR',`RxD',`RS0',`RS1',`Vcc',`/DCD',`/DSR',`D0',`D1',`D2',`D3',`D4',`D5',`D6',`D7',`/Irq',`Phi2',`R-/W'')

define(`Description_6801_dil', `micro controller')
define(`Param1_6801_dil', 40)
define(`Param2_6801_dil', 600)
define(`PinList_6801_dil', ``Vss',`XTAL1',`EXTAL2',`/NMI',`/IRQ1',`/Reset',`Vcc',`P20-Mode0',`P21-Mode1',`P22-Mode2',`P23-RxD',`P24-TxD',`P10',`P11',`P12',`P13',`P14',`P15',`P16',`P17',`Vcc-StdBy',`P47-A15',`P46-A14',`P45-A13',`P44-A12',`P43-A11',`P42-A10',`P41-A9',`P40-A8',`P37-A7-D7',`P36-A6-D6',`P35-A5-D5',`P34-A4-D4',`P33-A3-D3',`P32-A2-D2',`P31-A1-D1',`P30-A0-D0',`SC2-R-/W',`SC1-/AS',`E'')

define(`Description_81C17_dil', `UART')
define(`Param1_81C17_dil', 20)
define(`Param2_81C17_dil', 300)
define(`PinList_81C17_dil', ``D=',`D1',`/CS',`/Rd',`D2',`D3',`D4',`/Wr',`D5',`Gnd',`D6',`D7',`Clk',`/Int',`RS',`RxD',`TxD',`/CP1',`/CP2',`Vcc'')


define(`Description_AT90S1200_dil', `AVR Enhanced RISC microcontroller')
define(`Param1_AT90S1200_dil', 20)
define(`Param2_AT90S1200_dil', 300)
define(`PinList_AT90S1200_dil', ``/Reset',`PD0',`PD1',`XTAL2',`XTAL1',`PD2/INT0',`PD3',`PD4/T0',`PD5',`Gnd' ,`PD6',`PB0/AIN0',`PB1/AIN1',`PB2',`PB3',`PB4',`PB5/MOSI',`PB6/MISO',`PB7/S CK',`Vcc'')

define(`Description_AT90S1300_dil', `AVR Enhanced RISC microcontroller')
define(`Param1_AT90S1300_dil', 20)
define(`Param2_AT90S1300_dil', 300)
define(`PinList_AT90S1300_dil', ``/Reset',`PD0',`PD1',`XTAL2',`XTAL1',`PD2/INT0',`PD3',`PD4/T0',`PD5',`Gnd' ,`PD6',`PB0/AIN0',`PB1/AIN1',`PB2',`PB3',`PB4',`PB5/MOSI',`PB6/MISO',`PB7/S CK',`Vcc'')

define(`Description_AT90S2313_dil', `AVR Enhanced RISC microcontroller')
define(`Param1_AT90S2313_dil', 20)
define(`Param2_AT90S2313_dil', 300)
define(`PinList_AT90S2313_dil', ``/Reset',`PD0/RxD',`PD1/TxD',`XTAL2',`XTAL1',`PD2/INT0',`PD3/INT1',`PD4/T0 ',`PD5/T1',`Gnd',`PD6/ICP',`PB0/AIN0',`PB1/AIN1',`PB2/OC0',`PB3/OC1',`PB4', `PB5/MOSI',`PB6/MISO',`PB7/SCK',`Vcc'')

divert(0)dnl
