





## 使用场景

### 单独bridge vlan

```
[DESKTOP]<--->[eth0][bridge][eth1]<--->[PC]
```



> 1. 配置桥
>
>    ```bash
>    brctl addbr brg1
>    brctl addif brg1 eth0
>    brctl addif brg1 eth1
>    ifconfig brg1 up
>    ```
>
> 2. 从**DESKTOP** 向 **PC** 发送 vlan tag 5 的报文，当`vlan_filtering` 不开启的时，报文正常转发
>
> 3. 开启桥`vlan_filtering`，报文不转发
>
> 4. 配置vlan 之后正常转发
>
>    ```bash
>    bridge vlan add vid 5 dev eth0
>    bridge vlan add vid 5 dev eth1
>    bridge vlan add vid 5 dev brg1 self
>    ```





### bridge vlan 和 vlan 设备一起







## 附录

### 配置命令



### 配置接口

```bash
Usage: bridge vlan { add | del } vid VLAN_ID dev DEV [ tunnel_info id TUNNEL_ID ]
                                                     [ pvid ] [ untagged ]
                                                     [ self ] [ master ]
       bridge vlan { show } [ dev DEV ] [ vid VLAN_ID ]
       bridge vlan { tunnelshow } [ dev DEV ] [ vid VLAN_ID ]
```



| 参数     | 意义                            |
| -------- | ------------------------------- |
| dev NAME |                                 |
| vid VID  |                                 |
| pvid     |                                 |
| untagged |                                 |
| self     | 此处的这个slef ？？？           |
| master   | master ？？？究竟表示什么意思？ |
|          |                                 |



* 从配置上来看，**self** 只能伴随 桥 使用，**master**  只能伴随 桥中的端口 一起使用，否则下发配置时会报错
* 添加打印信息，查看内部实习？？？

