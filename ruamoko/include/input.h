#ifndef __ruamoko_input_h
#define __ruamoko_input_h

#include <QF/input.h>

struct plitem_s;
void IN_LoadConfig (struct plitem_s *config);
in_button_t *IN_CreateButton (string name, string description);
in_axis_t *IN_CreateAxis (string name, string description);
int IN_FindDeviceId (string _id);
string IN_GetDeviceName (int devid);
string IN_GetDeviceId (int devid);
//IN_AxisInfo ();
//IN_ButtonInfo ();
string IN_GetAxisName (int devid, int axis);
string IN_GetButtonName (int devid, int axis);
int IN_GetAxisNumber (int devid, string axis);
int IN_GetButtonNumber (int devid, string axis);
void IN_ProcessEvents (void);
void IN_ClearStates (void);
int IN_GetAxisInfo (int devid, int axis, in_axisinfo_t *info);
int IN_GetButtonInfo (int devid, int button, in_buttoninfo_t *info);
typedef void (*button_listener_t) (void *data, in_button_t *button);//FIXME const
@overload void IN_ButtonAddListener (in_button_t *button,
									 button_listener_t listener, void *data);
@overload void IN_ButtonRemoveListener (in_button_t *button,
										button_listener_t listener,
										void *data);
typedef void (*axis_listener_t) (void *data, in_axis_t *axis);//FIXME const
@overload void IN_AxisAddListener (in_axis_t *axis, axis_listener_t listener,
								   void *data);
@overload void IN_AxisRemoveListener (in_axis_t *axis,
									  axis_listener_t listener, void *data);
@overload void IN_ButtonAddListener (in_button_t *button, IMP listener,
									 id obj);
@overload void IN_ButtonRemoveListener (in_button_t *button, IMP listener,
										id obj);
@overload void IN_AxisAddListener (in_axis_t *axis, IMP listener, id obj);
@overload void IN_AxisRemoveListener (in_axis_t *axis, IMP listener, id obj);

int IMT_CreateContext (string name);
int IMT_GetContext (void);
void IMT_SetContext (int ctx);

#endif//__ruamoko_input_h
