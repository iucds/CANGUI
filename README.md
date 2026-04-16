# CANGUI
CAN Message Interpreter and Sender that has be proven to work on Windows

# Python Requirements:
Python version: Python 3.13
Python Packages: wxPython and python-can

# CPP Requirements:
CMake version >= 3.15
Toolchain: vcpkg
C++ version: 17
Libraries: wxWidgets (vcpkg install wxwidgets:x64-windows) and IXXAT Library
Installing VCI SDK:
1. Install this zip file and extract it: https://hmsnetworks.blob.core.windows.net/nlw/docs/default-source/products/ixxat/monitored/pc-interface-cards/vci-v4-0-1348-windows-11-10.zip?sfvrsn=2d1dfdd7_91
2. Run the installer and select to install the SDK

# 1. Setting up for Use:
1. Ensure that the IXXAT USB-to-CAN device is connected
2. Power up the CAN device that is to be interfaced with and connect it to the USB-to-CAN device. *Ensure that there is a terminating resistor or an error message may occur.
3. Start the GUI program

# 2. Using the GUI
## 2.1 Sending CAN messages
1. Key in the desired CAN ID and data bytes into the text fields that is to be sent to the CAN device
2. Press the send button. If the data is valid, there will be a green highlighted text that says that data has been sent successfully. Otherwise, there will be red highlighted text that says that it is an invalid data frame.
3. The LOG text will show the sent CAN ID, Data and also the interpreted CAN message.

## 2.2 Receiving CAN messages
1. When a CAN message is received, the GUI will decode the CAN message and identify the command as well as whether or not it is a sent or replied message

## 2.3 Configuration Mapping
1. Upon robot start up, it will send a lot of CAN messages which sends data related to its configuration. These messages are of command category 0 and command 1. This is "config aware".
2. When the GUI receives these config aware messages, it will store these messages into a dictionary.
3. To display the configuration mapping, click the LOG button on the top left corner to toggle to display the map.
4. To switch back to see the messages, click the same button again to switch back.

## 2.4 Saving MAP and LOG data
1. In the lower text field, key in the file directory in which the MAP and LOG data is to be saved.
2. Click the save button and a MAP.txt and LOG.txt file will be generated in that directory with all the MAP and LOG data.

## 2.5 Clearing LOG data
1. Click the clear button and the log field should be cleared.

# 3. Potential error messages

## can.interfaces.ixxat.exceptions.VCIDeviceNotFoundError: No IXXAT device(s) connected or device(s) in use by other process(es).
IXXAT device is not connected before starting the GUI. 
Connect the IXXAT device first before starting the GUI again.

## can.interfaces.ixxat.exceptions.VCIError: function canControlGetStatus failed (Attempt to send a message to a disconnected communication port.)
IXXAT device was connect when the GUI was started but then unplugged before sending a message. 
Connect the IXXAT device first before starting the GUI again.

## can.interfaces.ixxat.exceptions.VCIError: Error warning limit exceeded
The GUI tried to send a CAN message through the IXXAT device but the CAN device connected to it has not been configured properly.
Ensure the CAN device is configured properly with proper terminating resistors and restart the GUI.







