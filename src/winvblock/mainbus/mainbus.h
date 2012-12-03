#ifndef M_WV_MAINBUS_H_
/**
 * Copyright (C) 2009-2012, Shao Miller <sha0.miller@gmail.com>.
 * Copyright 2006-2008, V.
 * For WinAoE contact information, see http://winaoe.org/
 *
 * This file is part of WinVBlock, originally derived from WinAoE.
 *
 * WinVBlock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinVBlock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinVBlock.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * Main bus specifics
 */

/** Macros */
#define M_WV_MAINBUS_H_

/** Object types */
typedef struct S_WV_MAIN_BUS S_WV_MAIN_BUS;

/** Function declarations */

/* From mainbus.c */
extern NTSTATUS STDCALL WvBusRemoveDev(IN WV_S_DEV_T * Device);
extern NTSTATUS WvMainBusInitialBusRelations(DEVICE_OBJECT * DeviceObject);

/* From busirp.c */
extern VOID WvMainBusBuildMajorDispatchTable(void);
extern DRIVER_DISPATCH WvBusIrpDispatch;
extern WVL_F_BUS_PNP WvBusPnpQueryDevText;

/** Struct/union type definitions */

/** The bus extension type */
struct S_WV_MAIN_BUS {
    /** This must be the first member of all extension types */
    WV_S_DEV_EXT DeviceExtension[1];

    /** The lower device this FDO is attached to, if any */
    DEVICE_OBJECT * LowerDeviceObject;

    /** PnP bus information */
    PNP_BUS_INFORMATION PnpBusInfo[1];

    /** PnP bus relations */
    DEVICE_RELATIONS * BusRelations;

    /** Hack until proper PDO-add support is implemented */
    DEVICE_RELATIONS * BusRelationsHack;
  };

/** External objects */

/* From mainbus.c */
extern UNICODE_STRING WvBusDosName;
extern WVL_S_BUS_T WvBus;
extern WV_S_DEV_T WvBusDev;
extern volatile LONG WvMainBusCreators;

#endif /* M_WV_MAINBUS_H_ */
