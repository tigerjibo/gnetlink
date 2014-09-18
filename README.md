service_netlink
===============

A Netlink implementation based on the service type, and a python interface.


Code Introduction
=================
首先将当前目录切换到service_netlink目录中。
（1）执行 make，编译内核模块 test_netlink.ko；
（2）加载内核模块 test_netlink.ko；
（3）执行 ./build-netlink.sh，编译用户空间Python C扩展；
     注：如果是 Python2.7 版本，添加 2.7 参数，即 ./build-netlink.sh 2.7
（4）在Python代码中，导入netlink模块：
     import  netlink
    
     
API
===
参见docs目录。
