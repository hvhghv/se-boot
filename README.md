# se-boot
这是一款Linux脚本自启动程序，以最简单，快捷的方式实现开机自启,
同时也提供将程序转变为守护进程/后台程序的功能

## 特点
- [x] 快速将程序转变为守护进程/后台程序
- [x] 执行自启脚本一次后，即使再次运行也不会重新执行自启脚本 (意味着甚至可以丢到~/.bashrc中去也可正常工作)
- [x] 提供简单的日志记录
- [ ] 不支持后台程序/脚本的停止，重启功能

## 使用方式

### 将程序转变为后台程序

#### 命令
```
se-boot <command> // 其中<command> 不可以为"log/boot/help"
```

#### 示例
```
se-boot ls -a // 后台执行"ls -a"
se-boot python3 -m http.server -b 8080 // 后台执行"python3 -m http.server -b 8080"
```

### 脚本自启

#### 条件
将`/your/path/se-boot boot`加入到主机的自启命令中去，可通过`systemctl`, `openrc`, `/etc/inittab`等方式自行添加

#### 执行逻辑
执行`se-boot boot`后，会自动搜索/etc/se_boot文件夹（若不存在则创建）下的脚本并执行

#### 脚本格式
文件名为"<priority>_<timeout>_<name>.sh", 其中priority与timeout必须为2位数。priority值小的优先执行，值大的会等待值小的执行完或者超时后才执行，其值范围为00-99。timeout以秒为单位，范围为00-99，超时后停止等待脚本执行完成并选取其他脚本执行。

#### 注意
若要进行工作路径，环境变量，执行用户的切换，请在脚本内部编写相应内容（例如: cd/export/su）,se-boot并不提供相应的api。（工作路径，环境变量，执行用户均继承父进程）

#### 示例
下列是一个简单的脚本,文件名为`01_01_simple.sh`, 存放与/etc/se_boot中去
"""
#!/bin/bash

echo "hello_world"
ls -a
se-boot ls -a

su user -c "
cd /home/user
export A=xxx
se-boot python3 main.py
"
"""

### 日志
se-boot所有的后台程序输出，脚本输出都会记录在日志中
可通过`se-boot log` 查看最近30条的日志
若要查看所有日志，请查看`/var/se_boot/se_boot.log`与`/var/se_boot/se_boot_last.log`

### 编译
一般直接敲`make`就行
若要添加调试信息，敲`make DEBUG=1`
若要交叉编译，修改`mkenv.mk`文件，将`CROSS`参数修改为交叉工具链

