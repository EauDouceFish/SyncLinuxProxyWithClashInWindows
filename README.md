﻿# SyncLinuxProxyWithClashInWindows

Created 2024/8/4

A small script for syncronize Clash Proxy between Windows and Linux in VM.

Usage:
1.Open Windows-Clash and the Virtual Machine(SSH connected with host first).

2.Windows-Clash --System Proxy On, [ALLOW LAN True]

3.Set VM network mode using Bridge Network Card or sth like that. (Not NAT)

4.In Linux bash: [ip addr show], then find linux [static nerwork ip]

5.Open the script file, modify the parameter to yours: 

    (1)config.yaml file path
    
    (2)VM username
    
    (3)The IP get from step 4
    
6.Then click on the exe file or Debug use Ctrl+(Fn)F5, this small tool will do:

    (1)Connect to the virtual machine by ssh
    
    (2)Modify your VM's http(s)_proxy and all_proxy to your WLAN IP, use Clash port.
    
    (3)Clash config file also modified, the "external-controller": 127.0.0.1:xxxxx will be changed to your WLAN IP:xxxxx, syncronized with Linux settings.

Then Clash proxy will be available in VM-Linux.
If you want to change the settings, try nano ~/.bashrc and modify or delete the lines about proxys.
