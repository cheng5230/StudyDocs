# Git安装Step By Step #

安装一般都是选择默认路径，如果有固态硬盘的话最好安装在C盘，加快一下执行的速度。

打开Git的安装程序：

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094320335-1661335714.png)

点击Next之后

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094321960-1942472676.png)

安装路径我保持默认选项，然后继续下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094323460-632789432.png)

继续下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094325241-2069380358.png)

提示你创建开始菜单，我们继续下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094326882-443733845.png)

上图默认的是第二项，但是我改成了第一项，不想在CMD下使用Git命令。

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094328491-991209542.png)

继续下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094331304-762634881.png)

继续下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094333226-711592818.png)

下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094335054-1005312342.png)

下一步

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094336710-998129506.png)

安装完毕

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094339038-1315819532.png)


# Git的配置Step By Step #

Git安装成功之后，我们验证一下是否安装成功。

Win+R打开CMD之后，输入git

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094341804-261880149.png)

发现并没有git这个命令，这里我们要将C:\Program Files\Git\bin加入到环境变量中，这一步的演示就省略了


环境变变量设置之后，再试一次

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094343585-468018676.png)

发现git命令已经可以正常使用了。

从开始菜单打开Git Bash：

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094345179-1617334090.png)

然后可以看到如下界面

![](https://images2017.cnblogs.com/blog/806053/201709/806053-20170909094346882-2033109501.png)

# Git配置 #
安装成功后，打开git bash。

1、设置使用GIT 时的姓名和邮箱地址。名字请用英文输入。
（在 GITHUB上公开仓库时，这里的姓名和邮箱地址也会随着提交日志一同被公开）

$ git config --global user.name "ShareCheng"

$ git config --global user.email "541397265@qq.com"

2、提高命令输出的可读性。将 color.ui  设置为 AUTO 
  可以让命令的输出拥有更高的可读性。

$ git config --global color.ui auto

3、设置SSH KEY。GITHUB 上连接已有仓库时的认证，
   是通过使用了 SSH 的公开密钥认证方式进行的。

$ ssh-keygen -t rsa -C "541397265@qq.com"

在bash输入后如下图，id_rsa文件是私有密钥，id_rsa.pub是公开密钥

![](https://i.imgur.com/nqrXa34.png)

4、在github中添加公开密钥
公钥也可以用命令查看

$ cat ~/.ssh/id_rsa.pub

在github中登录自己的账户，点击头像旁边的倒立三角图标，选择setting菜单，添加刚生成的公钥。

![](https://i.imgur.com/U6tHpzv.png)

5、验证
完成以上设置后，就可以用手中的私人密钥与 GitHub 进行认证和通信了。验证一下，输入命令：

$ ssh -T git@github.com

在输入yse，出现如下结果就成功了:
Hi hirocastest! You've successfully authenticated, but GitHub does not provide shell access.


附：
git的配置文件gitconfig一共有3个，
以win7_64bit为例：

仓库级配置文件。在当前仓库路径/.git/config

全局配置文件。路径：C:/Users/用户名称/.gitconfig

系统级配置文件。git安装路径下，../Git/mingw64/etc

配置文件的权重是仓库>全局>系统。如果定义的值有冲突，以权重值高的文件中定义的为准。

查看当前生效的配置，命令：git config -l，

这个时候会显示最终三个配置文件计算后的配置信息。



