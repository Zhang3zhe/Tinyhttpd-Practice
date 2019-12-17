﻿# Tinyhttpd Practice
Tinyhttpd 是 J. David Blackstone 于 1999 年写的一个约 500 行的超轻量型 Http Server,可以帮助我们真正理解服务器程序的本质,非常适合作为学习网络编程的入门级项目。

官网：https://sourceforge.net/projects/tinyhttpd/

以下内容来自原作者：

This software is copyright 1999 by J. David Blackstone. Permission is granted to redistribute and modify this software under the terms of the GNU General Public License, available at http://www.gnu.org/ .

If you use this software or examine the code, I would appreciate knowing and would be overjoyed to hear about it at jdavidb@sourceforge.net .

This software is not production quality. It comes with no warranty of any kind, not even an implied warranty of fitness for a particular purpose. I am not responsible for the damage that will likely result if you use this software on your computer system.

I wrote this webserver for an assignment in my networking class in 1999. We were told that at a bare minimum the server had to serve pages, and told that we would get extra credit for doing "extras." Perl had introduced me to a whole lot of UNIX functionality (I learned sockets and fork from Perl!), and O'Reilly's lion book on UNIX system calls plus O'Reilly's books on CGI and writing web clients in Perl got me thinking and I realized I could make my webserver support CGI with little trouble.

Now, if you're a member of the Apache core group, you might not be impressed. But my professor was blown over. Try the color.cgi sample script and type in "chartreuse." Made me seem smarter than I am, at any rate. :)

Apache it's not. But I do hope that this program is a good educational tool for those interested in http/socket programming, as well as UNIX system calls. (There's some textbook uses of pipes, environment variables, forks, and so on.)

One last thing: if you look at my webserver or (are you out of mind?!?) use it, I would just be overjoyed to hear about it. Please email me. I probably won't really be releasing major updates, but if I help you learn something, I'd love to know!

Happy hacking!
    
J. David Blackstone


## 运行效果
首先使用make命令对httpd.c进行编译，开启服务器；然后打开浏览器，输入IP地址和服务器端口号，回车一下，就可以看到服务器传回的html页面了。如果浏览器显示空白，需要修改一下index.html文件的权限。

![httpd](https://github.com/Zhang3zhe/Tinyhttpd-Practice/blob/master/image/httpd.png)

接下来可以使用自带的客户端程序进行实验，先开启服务器程序，然后在另一终端里面编译client.c，运行客户端程序，输入服务器的端口号进行连接。客户端向服务器发送字母，然后服务器再将字母传回。

![client](https://github.com/Zhang3zhe/Tinyhttpd-Practice/blob/master/image/client.png)

## Acknowledgement & Reference
* [1] https://github.com/EZLippi/Tinyhttpd

* [2] https://github.com/wanttobeno/Tinyhttpd

* [3] https://github.com/gosth/Tinyhttpd

* [4] https://github.com/cbsheng/tinyhttpd

* [5] https://github.com/nengm/Tinyhttpd

* [6] https://github.com/nameistw/TinyHTTPd
