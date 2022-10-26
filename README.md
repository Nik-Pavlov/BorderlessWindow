Borderless Window
================

This sample application demonstrates differences in Windows 10 and Windows 11 [DwmExtendFrameIntoClientArea](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmextendframeintoclientarea) implementations and how it creates discrepancies in how the borderless window looks.

Additionally, these discrepancies result in a 1 px gap when the window is Maximized in Windows 11.

### Windows 10: 1 px white line on top of the window
![image](https://user-images.githubusercontent.com/30529656/198061778-691f46cb-840b-4cb3-998e-e7821d805388.png)

### Windows 11: No lines on top of the window
![image](https://user-images.githubusercontent.com/30529656/198061926-1c608884-bebb-4fe2-83cf-46b95ebaa5dc.png)

### Windows 10: Maximized
![image](https://user-images.githubusercontent.com/30529656/198062043-5c0b7034-2a55-4525-8a81-5e9ad22fc086.png)

### Windows 11: Maximized - 1px gap between the taskbar and the window
![image](https://user-images.githubusercontent.com/30529656/198062519-a66c11aa-8f71-43fb-992a-50b5ced858f8.png)
