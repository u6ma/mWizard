/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#ifndef _WM_MONITOR_DLG_H
#define _WM_MONITOR_DLG_H

/*
 * Posts mWmonitor, creating it on first use. Bound to f.monitors, reachable
 * from mWand over _MWIZARD_COMMAND, and posted on hotplug when
 * monitorDialogOnHotplug is set.
 */
void PostMonitorDialog(void);

/*
 * Registers mWizard's willingness to be asked for the window, and restores a
 * saved layout if one matches the monitors that are attached. Called once from
 * InitWmScreen()'s init block.
 */
void InitMonitorDialog(void);

#endif /* _WM_MONITOR_DLG_H */
