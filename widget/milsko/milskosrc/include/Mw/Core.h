/*!
 * @file Mw/Core.h
 * @brief Core
 */
#ifndef __MW_CORE_H__
#define __MW_CORE_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

/*!
 * @warning Used internally
 * @brief Dispatches the handler of widget class
 * @param x Widget
 * @param y Handler name
 */
#define MwDispatch(x, y) \
	if(!x->destroyed && x->widget_class != NULL && x->widget_class->y != NULL) x->widget_class->y(x)

/*!
 * @warning Used internally
 * @brief Dispatches the handler of widget class
 * @param x Widget
 * @param y Handler name
 * @return `0` for success, otherwise failed
 */
#define MwDispatch2(x, y) \
	((!x->destroyed && x->widget_class != NULL && x->widget_class->y != NULL) ? x->widget_class->y(x) : 0)

/*!
 * @warning Used internally
 * @brief Dispatches the handler of widget class
 * @param x Widget
 * @param y Handler name
 * @param z Argument
 */
#define MwDispatch3(x, y, z) \
	if(!x->destroyed && x->widget_class != NULL && x->widget_class->y != NULL) x->widget_class->y(x, z)

/*!
 * @warning Used internally
 * @brief Dispatches the handler of widget class
 * @param x Widget
 * @param y Handler name
 * @param z Argument
 * @param w Argument
 */
#define MwDispatch4(x, y, z, w) \
	if(!x->destroyed && x->widget_class != NULL && x->widget_class->y != NULL) x->widget_class->y(x, z, w)

#define MwWaitMS 30

#define MwDoubleClickTimeout 250

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Initializes the Milsko Toolkit
 * @return `0` if successful
 */
MWDECL int MWAPI MwLibraryInit(void);

/*!
 * @brief Creates a widget
 * @param widget_class Widget class
 * @param name Widget name
 * @param parent Parent widget or `NULL`
 * @param x X
 * @param y Y
 * @param width Width
 * @param height Height
 * @return Widget
 */
MWDECL MwWidget MWAPI MwCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height);

/*!
 * @brief Creates a widget
 * @param widget_class Widget class
 * @param name Widget name
 * @param parent Parent widget or `NULL`
 * @param x X
 * @param y Y
 * @param width Width
 * @param height Height
 * @param ... Same with MwVaApply
 * @return Widget
 */
MWDECL MwWidget MWAPI MwVaCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, ...);

/*!
 * @brief Creates a widget
 * @param widget_class Widget class
 * @param name Widget name
 * @param parent Parent widget or `NULL`
 * @param x X
 * @param y Y
 * @param width Width
 * @param height Height
 * @param va Same with MwVaListApply
 * @return Widget
 */
MWDECL MwWidget MWAPI MwVaListCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, va_list va);

/*!
 * @brief Destroys the widget and its child widgets
 * @param handle Widget
 */
MWDECL void MWAPI MwDestroyWidget(MwWidget handle);

/*!
 * @brief Executes a method specific to the widget (varadic version).
 * @warning Prefer using corresponding functions for better code clarity and type safety.
 * @param handle Widget
 * @param ... Widget function arguments.
 */
MWDECL void MWAPI MwVaWidgetExecute(MwWidget handle, const char* func_name, void* out, ...);

/*!
 * @brief Executes a method specific to the widget (va_list version).
 * @warning Prefer using corresponding functions for better code clarity and type safety.
 * @param handle Widget
 * @param va Widget function arguments.
 */
MWDECL void MWAPI MwVaListWidgetExecute(MwWidget handle, const char* func_name, void* out, va_list va);

/*!
 * @brief Runs the main loop
 * @param handle Widget
 */
MWDECL void MWAPI MwLoop(MwWidget handle);

/*!
 * @brief Runs the single step
 * @param handle Widget
 * @return `0` if successful
 */
MWDECL int MWAPI MwStep(MwWidget handle);

/*!
 * @brief Check if any event is pending
 * @param handle Widget
 * @return `1` if any event is pending
 */
MWDECL int MWAPI MwPending(MwWidget handle);

/*!
 * @brief Sets an integer property
 * @param handle Widget
 * @param key Key
 * @param n Value
 */
MWDECL void MWAPI MwSetInteger(MwWidget handle, const char* key, int n);

/*!
 * @brief Sets a text property
 * @param handle Widget
 * @param key Key
 * @param value Value
 */
MWDECL void MWAPI MwSetText(MwWidget handle, const char* key, const char* value);

/*!
 * @brief Sets a void pointer property
 * @param handle Widget
 * @param key Key
 * @param value Value
 */
MWDECL void MWAPI MwSetVoid(MwWidget handle, const char* key, void* value);

/*!
 * @brief Gets the integer property
 * @param handle Widget
 * @param key Key
 * @return Value
 */
MWDECL int MWAPI MwGetInteger(MwWidget handle, const char* key);

/*!
 * @brief Gets the text property
 * @param handle Widget
 * @param key Key
 * @return Value
 */
MWDECL const char* MWAPI MwGetText(MwWidget handle, const char* key);

/*!
 * @brief Gets the void pointer property
 * @param handle Widget
 * @param key Key
 * @return Value
 */
MWDECL void* MWAPI MwGetVoid(MwWidget handle, const char* key);

/*!
 * @brief Sets the default property
 * @param handle Widget
 * @warning This is called when widget is created
 */
MWDECL void MWAPI MwSetDefault(MwWidget handle);

/*!
 * @brief Sets the properties
 * @param handle Widget
 * @param ... Properties
 */
MWDECL void MWAPI MwVaApply(MwWidget handle, ...);

/*!
 * @brief Sets properties
 * @param handle Widget
 * @param va Properties
 */
MWDECL void MWAPI MwVaListApply(MwWidget handle, va_list va);

/*!
 * @brief Sets a user handler
 * @param handle Widget
 * @param key Key
 * @param handler Handler
 * @param user_data User data passed to handler
 */
MWDECL void MWAPI MwAddUserHandler(MwWidget handle, const char* key, MwUserHandler handler, void* user_data);

/*!
 * @brief Dispatches the user handler
 * @param handle Widget
 * @param key Key
 * @param handler_data Handler data passed to handler
 */
MWDECL void MWAPI MwDispatchUserHandler(MwWidget handle, const char* key, void* handler_data);

/*!
 * @brief Sets an error handler
 * @param handle Widget
 * @param handler Handler
 * @param user_data User data passed to handler
 */
MWDECL void MWAPI MwSetErrorHandler(MwErrorHandler handler, void* user_data);

/*!
 * @brief Dispatches the error handler
 * @param code Error code
 * @param message Error message
 */
MWDECL void MWAPI MwDispatchError(int code, const char* message);

/*!
 * @brief Forcefully makes widget render
 * @param handle Widget
 */
MWDECL void MWAPI MwForceRender(MwWidget handle);

/*!
 * @brief Forcefully makes widget render
 * @param handle Widget
 * @param ptr Ignored
 */
MWDECL void MWAPI MwForceRender2(MwWidget handle, void* ptr);

/*!
 * @brief Adds an widget to tick handler list
 * @param handle Widget
 */
MWDECL void MWAPI MwAddTickList(MwWidget handle);

/*!
 * @brief Focus the widget
 * @param handle Widget
 */
MWDECL void MWAPI MwFocus(MwWidget handle);

/*!
 * @brief Grabs the pointer
 * @param handle Widget
 * @param toggle Toggle
 */
MWDECL void MWAPI MwGrabPointer(MwWidget handle, int toggle);

/*!
 * @brief Hides the cursor
 * @param handle Widget
 */
MWDECL void MWAPI MwHideCursor(MwWidget handle);

/*!
 * @brief Toggles the dark theme
 * @param handle Widget
 * @param toggle Toggle
 * @deprecated Use MwNdarkTheme instead
 */
MWDECL void MWAPI MwSetDarkTheme(MwWidget handle, int toggle);

/*!
 * @brief Gets the parent widget
 * @param handle Widget
 * @return Parent widget
 */
MWDECL MwWidget MWAPI MwGetParent(MwWidget handle);

/*!
 * @brief Show widget
 * @param handle Widget
 * @param toggle Toggle
 */
MWDECL void MWAPI MwShow(MwWidget handle, int toggle);

/*!
 * @brief Reparents widget
 * @param handle Widget
 * @param new_parent New parent
 */
MWDECL void MWAPI MwReparent(MwWidget handle, MwWidget new_parent);

/*!
 * @brief Gets class of widget
 * @param handle Widget
 * @return Class
 */
MWDECL MwClass MWAPI MwGetClass(MwWidget handle);

/*!
 * @brief Gets name of widget
 * @param handle Widget
 * @return Name
 */
MWDECL const char* MWAPI MwGetName(MwWidget handle);

/*!
 * @brief Gets children of widget
 * @param handle Widget
 * @return Children (NULL-terminated array)
 */
MWDECL MwWidget* MWAPI MwGetChildren(MwWidget handle);

/*!
 * @brief Gets the cursor coordinate
 * @param handle Widget
 * @param point Point
 */
MWDECL void MWAPI MwGetCursorCoord(MwWidget handle, MwPoint* point);

/*!
 * @brief Gets the screen size
 * @param handle Widget
 * @param rect Rectangle
 */
MWDECL void MWAPI MwGetScreenSize(MwWidget handle, MwRect* rect);

/*!
 * @brief Reports whether a widget reports global or local coordinates upon GetXY/SetXY. Anything with a parent reports local, and most backends report global coordinates being supported for top level windows, but some (Wayland) do not.
 * @param handle Widget
 */
MWDECL int MWAPI MwGetCoordinateType(MwWidget handle);

/*!
 * @brief Queue to get clipboard content
 * @param handle Widget
 * @param clipboard_type The Clipboard Type to get. See MwClipboardType. This is ignored on backends that aren't X11/Wayland, so if you're unsure, just use MwClipboardMain
 */
MWDECL void MWAPI MwGetClipboard(MwWidget handle, int clipboard_type);

/*!
 * @brief Set user pointer
 * @param handle Widget
 * @param user User pointer
 */
MWDECL void MWAPI MwSetUser(MwWidget handle, void* user);

/*!
 * @brief Get user pointer
 * @param handle Widget
 * @return User pointer
 */
MWDECL void* MWAPI MwGetUser(MwWidget handle);

/*!
 * @brief Free the widget
 * @param handle Widget
 * @warning This is used internally!
 * @note You can call this on toplevel widget safely if you're sure no event loops will be ran, e.g. after `MwLoop`.
 */
MWDECL void MWAPI MwFreeWidget(MwWidget handle);

/*!
 * @brief Gets the frame of the widget
 * @param handle Widget
 * @param rect Frame
 */
MWDECL void MWAPI MwGetFrame(MwWidget handle, MwRect* rect);

#ifdef _MILSKO
MWDECL void MwForceRender_Internal(MwWidget handle);
MWDECL void MwForceRender2_Internal(MwWidget handle, void* ptr);

#define MwForceRender MwForceRender_Internal
#define MwForceRender2 MwForceRender2_Internal
#endif

#ifdef __cplusplus
}
#endif

#endif
