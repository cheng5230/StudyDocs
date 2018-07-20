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



# 上传文件/项目到Github #

1.注册并登陆Github。

2.登陆进去之后的页面，点击这个“库”，这表示你在Github上上的代码仓库，我这里已经创建过一个了，所以数量是1

![](https://i.imgur.com/JBdjWdm.png)

3.在仓库选项卡中，点击“新建”按钮添加一个项目。

![](https://i.imgur.com/fMfI9X8.png)

4.为了不废话我就翻译成了中文页面，这里填写好项目的信息。

![](https://i.imgur.com/MY2rgit.png)

5.创建好项目之后，在项目界面点击右边的“克隆或下载”，复制这个URL，待会会用到。

![](https://i.imgur.com/N4IKc3v.png)

6.在本地新建一个项目文件夹，在Git bash中cd进这个文件夹，TortoiseGit的操作方式是：

![](https://i.imgur.com/ME3Z5ZS.png)

![](https://i.imgur.com/Wc1pudq.png)

7.命令git clone https://github.com/makeiot/test.git（这里把URL换成你刚刚复制的自己项目的）

![](https://i.imgur.com/X3rVYVj.png)

8.现在在你的这个目录下就多了一个以你在Github上上上的项目名字命名的文件夹了

![](https://i.imgur.com/FVoLlBx.png)

9.把你要上传的整个项目放入这个文件夹，如：

![](https://i.imgur.com/NlNQLGd.png)

10.在bash中进入这个目录cd test（你项目所在的文件目录）

然后再依次执行命令：

git add .  （注意：add后面是空格加.）

git commit -m“描述”  （注意：“ 描述 ”里面换成你需要，如“测试版本”）

git push -u origin master（注意：此操作目的是把本地仓库push到github上面，此步骤需要你输入帐号和密码）

![](https://i.imgur.com/c4sFBxt.png)

11 通过以上步骤，一个项目就成功上传到Github上上啦，登录查看一下：

![](https://i.imgur.com/M8erfZf.png)



# Git连接GitHub时遇到问题处理 #

git 连接github 遇到fatal：HttpRequestException encountered问题是因为 Github 禁用了TLS v1.0 and v1.1，必须更新Windows的git凭证管理器。通过以下网址下载 GCMW-1.17.0-preview.3.exe

https://github.com/Microsoft/Git-Credential-Manager-for-Windows/releases/


![](https://i.imgur.com/NnEkQWL.jpg)



下载之后安装重启git窗口 说明你的git和github已经可以同步了。

![](https://i.imgur.com/teBzxVu.jpg)






