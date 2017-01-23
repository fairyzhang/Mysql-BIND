# Mysql_BIND
## Introduction
This is a DNS system for easy management configuration which has been run for about 4 years at RenRen Inc.. It is based on BIND-9.9.2-P1 and the great difference from original BIND is using Mysql database instead of VIEW configuration.

Mysql_BIND can provide an intelligent DNS system which switches domain name resolution among different network operators or locations easily and rapidly . And the records of name zone are storing only once in Mysql database, not to configure each time for each VIEW in original BIND.

There are tow types of database tables : IP table and domain name resolution table. And each type of table has three important elements: network opeation , location of user IP and Internet Data Center for hoping resolution of user DNS querying which could assosiate user IP and resolution results.

Mysql_BIND also provide a resolution priority for domain name to ensure responding the best resolution in limit resources to user DNS query.

## Installation

1. Install the libeary of Mysql.

2. Edit 'bin/named/Makefile.in' file:

	1) Change the value of 'DBDRIVER_INCLUDES' into the result of executing 'mysql_config --cflags' command.

		(Ex. DBDRIVER_INCLUDES = -I/usr/include/mysql  -g -pipe -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC   -DUNIV_LINUX -DUNIV_LINUX) 

	2) Change the value of 'DBDRIVER_LIBS' into the result of executing 'mysql_config --libs' command.

		(Ex. DBDRIVER_LIBS = -rdynamic -L/usr/lib64/mysql -lmysqlclient -lz -lcrypt -lnsl -lm -lssl -lcrypto)

3. ./configure

	You can specify the install root directory by '--prefix' option. You can also run './configure --help' for more libraries help.

4. make

5. make install

## Documents

English:	file 'README.en'

Chinese:	file 'README.ch'

original:	file 'README.original'
