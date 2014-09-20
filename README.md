gnetlink
===============

An example of the Netlink and the Generic Netlink, and their python interface.


Netlink
=======
首先将当前目录切换到service_netlink目录中。
（1）执行 make，编译内核模块 test_netlink.ko；
（2）加载内核模块 test_netlink.ko；
（3）执行 ./build-netlink.sh，编译用户空间Python C扩展；
     注：如果是 Python2.7 版本（默认2.6），添加 2.7 参数，即 ./build-netlink.sh 2.7
（4）在Python代码中，导入netlink模块：
     import netlink
（5）在Python代码中，使用 netlink 模块中的功能。


Generic Netlink
===============
首先将当前目录切换到test_genl目录中。
（1）执行 make，编译内核模块 test_genl.ko；
（2）加载内核模块 test_genl.ko；
（3）执行 ./build-genl.sh，编译用户空间Python C扩展；
     注：如果是 Python2.7 版本（默认2.6），添加 2.7 参数，即 ./build-netlink.sh 2.7
（4）在Python代码中，导入genl模块：
     import genl
（5）在Python代码中，使用 genl 模块中的功能。


API
===
参见docs目录。
