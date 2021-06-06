## 问题一、新旧__br_fdb_get() 差异

| 版本号  | 函数原型                                                     |
| ------- | ------------------------------------------------------------ |
| 2.6.35  | struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,<br>const unsigned char *addr) |
| 4.4.155 | struct net_bridge_fdb_entry *\_\_br_fdb_get(struct net_bridge *br,<br>const unsigned char *addr,<br>\_\_u16 vid) |



2.6 内核中并没有桥vlan 过滤功能，所以在查询fdb 表的时候不需要有vid 参数。



桥中fdb 表的 vid 是如何赋值的？



## 附录

### 重要函数

| 函数名称        | 函数作用   |
| --------------- | ---------- |
| br_fdb_update() | 更新fdb 表 |
|                 |            |
|                 |            |



### 配置接口

```bash
Usage: bridge vlan { add | del } vid VLAN_ID dev DEV [ tunnel_info id TUNNEL_ID ]
                                                     [ pvid ] [ untagged ]
                                                     [ self ] [ master ]
       bridge vlan { show } [ dev DEV ] [ vid VLAN_ID ]
       bridge vlan { tunnelshow } [ dev DEV ] [ vid VLAN_ID ]
```











