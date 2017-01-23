# Mysql_BIND
## 简介
Mysql_BIND是一种能够轻松配置的DNS工具，他是基于BIND进行开发的。
Mysql_BIND与BIND的最大区别就是利用Mysql数据库进行DNS的配置管理，替代了传统而复杂的VIEW形式的配置管理。
Mysql_BIND在复杂多变的网络环境下，能够灵活，快速的进行DNS切换管理。域名解析所对应的来源IP粒度可以随意调整，而无需进行复杂的ACL和zone添加删除。
Mysql_BIND不需要为每个VIEW指定一次zone的定义，而仅仅需要定义一次zone即可，它可以适用于所有的来源IP地址。并可以针对不同来源IP的用户返回不同的解析结果。
Mysql_BIND定义了一种自身的优先级匹配策略，所有解析的数据条目仅存储一次就可以了，使得访问用户都可以响应到有限资源中最优的解析结果。减少数据库内冗余数据的出现，极大方便和简化运维操作。

## 原理
Mysql_BIND的实现保留了原生BIND的所有功能，在此基础上增加了利用数据库来存储用户来源IP和域名解析数据的相关逻辑。如果用户的访问需要利用数据库存储的数据进行解析，就需要配置两种数据库表：1）IP地址段表；2）域名解析信息表。IP地址段表是针对所有利用数据库解析的域名有效的，需要进行全局配置。而域名解析信息表，可以配置在VIEW中，也可以不配置在VIEW中。解析的具体返回还是要根据IP地址段表中的定义进行返回。因此如果想利用数据库进行相应解析，这两种数据库表是缺一不可的。

Mysql_BIND的逻辑是先根据IP地址段表进行用户IP的定位，从这张表中可以定位出用户的1)来源运营商，2)地域以及3)期望解析到的Internet机房。然后利用IP定位到了这三个元素，查找具体的域名解析表进行相应的域名解析。域名解析表中的每条记录也都会有1）归属运营商，2）所属地域，3）所属Internet机房这三种信息，这样可以使得不同来源IP的用户能够解析到不一样的解析结果。

域名解析表中所有数据均只存储一遍，这与传统VIEW的解析方式是完全不同的，因此也就需要根据用户IP的定位信息，在域名解析表中查找最匹配的解析信息。这个解析信息的匹配是有优先级的，从高到低分别是1）相同的Internet机房；2）相同的运营商和地域；3）相同的运营商；4）相同的地域；5)所有域名解析信息；6）泛域名解析信息。

前面提到的1）运营商，2）地域，和3）Internet机房这三个元素是Mysql_BIND的核心，最好的情况是这三种元素都是真实的，这样用户的DNS解析肯定会得到最优的结果，但是很多时候我们的机房覆盖没有那么广泛，就需要人为来设置一些解析的对应规则，使用户能够访问到有限资源中的最优结果。因此对于IP表中的这三项，是可以通过伪造，使用户能够解析到我们想要的机房或运营商。

Mysql_BIND的用户来源IP判断，是利用存储在数据库的IP地址段数据进行判断的，而且数据库中可以存放多个相同结构的IP表，来为不同域名进行特殊的定位。IP地址的定位也是根据数据库存储的IP表进行优先级匹配的，它的匹配顺序是1）域名独享的IP表；2）zone下所有域名共享的IP表；3）所有域名默认的IP表。在进行数据库存储的时候，命名是有规则的，域名独享的IP表命名规范是以'ip_'开头，后面添加将域名中的.替换成_的结果。zone下所有域名共享的IP表，命名规范与前面相似，以'ip_'开头，后面接zone名中将.替换成_的结果。而所有域名默认的IP表则固定命名为'ip_tbl'。对于数据库中所有以'ip_'开头的表都是IP表，它会在Mysql_BIND服务启动以后，将数据一次性读入到内存，作为后续解析使用。一般IP表中的数据是按照IP段进行存储的，可以通过whois进行信息收集，也可以通过一些通用的IP地址段网站进行下载（例如：apnic.net,ripe.net,arin.net等）。对于一些特殊的IP段，需要特殊进行配置，可以添加一个更小的IP地址段进行设置，IP的定位都是以最小段为原则进行定位的。但如果出现同一个IP段多次出现在一张IP表的情况下，只会以第一次出现的数据条目为准。另外，每条数据都需要定义前面说的三个重要信息，即：1）来源运营商，2）地域以及3）期望解析到的Internet机房，这是智能解析的关键。

进行了IP地址的定位以后，会获取到用户来源的三个信息1）运营商，2）地域以及3）期望解析到的Internet机房。这三个数据均可以有某个数据为0，如果为0，表示相应的解析优先级可以向下跳转到非空的判断中。例如某个IP地址段的信息中Internet机房字段为0，表示该来源IP的用户需要直接查找相同运营商和相同地域的解析信息。如果某个IP段的Internet机房字段和地域字段均为0，则表示该来源IP的用户需要使用相同运营商的记录进行解析返回，而相同运营商的记录很有可能会使用多个机房。因此对于同一地域的用户请求可以使用多个机房的解析结果，这种配置的方式就显得尤为重要和灵活。

用户的请求利用IP表定位到了上述三个信息之后，就需要到域名解析表中查找相应的解析结果。域名解析表是以zone为粒度进行存储的，同一个zone需要使用同一个域名解析表。这个域名表也是有固定的命名规则的，将zone名的.替换成_存储到数据库中。每个域名解析表，必须要存储一条SOA记录，且该记录不能出错，否则整个zone的加载就会出错。每个zone还需要有相应的A记录，NS记录等等，这些与原生BIND的文件配置基本相同，只是这些记录要以数据库的形式存放，但需要注意的是，域名解析表中的域名记录是以完整域名进行记录的。每条记录都有对应的1）归属运营商，2）所属地域，以及3）所属Internet机房这三种信息，这与IP表的信息类似，也是进行智能解析的重要元素。当然，域名解析表的这三种信息也有可以为0的，只是这里的0表示的ANY，即任意来源均可对应的意思，与IP表的含义是不同的。例如只能存在一条的SOA记录，它的上述三个元素便可以设置为0，任意来源均可直接命中。当然即使不为0，由于只有一条，也会根据匹配的优先级定位到它。

Mysql_BIND目前还只支持到IPV4的解析，主要针对的是常用的A,NS,CNAME等类型，对于复杂的DNSSEC还没有完全支持。


## 编译方法
1.安装Mysql相关库
2.修改bin/named/Makefile.in文件
	1) 将“mysql_config --cflags”命令的执行内容，替换DBDRIVER_INCLUDES的内容，例如（DBDRIVER_INCLUDES = -I/usr/include/mysql  -g -pipe -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC   -DUNIV_LINUX -DUNIV_LINUX）。
	2) 将"mysql_config --libs"命令的执行内容，替换DBDRIVER_LIBS的内容，例如（DBDRIVER_LIBS = -rdynamic -L/usr/lib64/mysql -lmysqlclient -lz -lcrypt -lnsl -lm -lssl -lcrypto）。
3../configuration 
4.make
5.make install


## 配置方法

### named.conf文件配置
添加IP库的Mysql连接方式以及域名解析相关的zone的Mysql连接信息。
mysqlipdb { #此配置一定要写在最顶层的{}内，不可写在VIEW内，如果没有此层配置，或是配置有问题，域名解析将返回所有域名指向，无法进行智能解析。
#       host "localhost"; 			#主机信息，默认为localhost
#       port 3306;					#端口信息，默认为3306端口
        database "mysql_bind";		#数据库信息，需要自行定义数据库
        user "root";				#用户信息，默认为root
        password "";				#密码信息，默认为空
};

zone "myexample.com"{ 
  type master;
  notify no;
  database "mysqldb mysql_bind localhost user passwd "; #mysqldb是固定的，表示使用mysql存储的域名进行解析。其他参数分别是数据库名，连接主机，连接用户名，用户密码（表名固定使用zone名中将.替换成_的值）
};

与数据库相关的所有日志，是打印到一个新的日志分类‘database’，如果需要查看相关日志，可以配置相关的日志信息，例如：
logging {
        channel db_log { 				#日志打印的频道，定义一个全新的名字，频道内的的所有配置与其他日志无异
                file "var/run/named/named.db" versions 3 size 20m;	
                severity info;
                print-time 1;
                print-severity 1;
                print-category 1;
        };

        category database {				#数据库的日志分类
                db_log;
        };
};


### 数据库配置
数据库中共有3种类型的表：1）IP表；2）域名解析表，与zone对应；3）管理表，与运维相关的管理系统有关。

#### IP地址段表
IP表的表结构都是相同，但会在表名上有一些差别，共有三种IP表:
1. 默认的IP表 ‘ip_tbl’，它的作用是针对所有域名解析有效。
2. zone下所有域名共用的IP表，它针对zone下的所有非特殊要求的域名解析有效，命名规则是以'ip_'开头，后面接zone名中将.替换成_的值。（例如ip_myexample_com）
3. 某个域名独有的IP表，它针对这个域名下的解析有效，命名规则也是以'ip_'开头，后面接域名中将.替换成_的值。（例如 ip_a_myexample_com）
当进行IP定位时，按照上述3）2）1）的顺序进行定位。
IP表定义如下，表名需要根据上述规则进行变更。（以ip_tbl为例）
CREATE TABLE `ip_tbl` (												#IP表名可以根据上述规则进行变更	
  `id` int(11) NOT NULL auto_increment,								#主键
  `sip` int(10) unsigned NOT NULL default '0',						#IP段的起始IP，以十进制数字形式存放
  `mask` int(11) NOT NULL default '0',								#IP段的子网掩码
  `real_isp_id` int(11) NOT NULL default '0',						#真实的运营商ID
  `real_location_id` int(11) NOT NULL default '0',					#真实的地域ID
  `isp_id` int(11) NOT NULL default '0',							#伪造的运营商ID
  `location_id` int(11) NOT NULL default '0',						#伪造的地域ID
  `idc_id` int(11) NOT NULL default '0',							#Internet机房信息ID
  PRIMARY KEY  (`id`)
) DEFAULT CHARSET=utf8;
其中，isp_id, location_id, idc_id 是Mysql_BIND解析时实际使用的，real_isp_id, real_location_idi只作为数据存储时的参考。
IP地址的转换可以使用mysql自带的函数：inet_aton将字符串IP地址转换成无符号十进制整型数; inet_ntoa将十进制数值转换成字符串形式的IPV4地址。

#### 域名解析表
域名表按照zone进行区分，命名方式是将zone的名称中.替换成_的值。
myexample.com的解析表定义如下：
CREATE TABLE `myexample_com` (										#myexample_com为myexample.com对应的域名解析表
  `id` int(11) NOT NULL auto_increment,								#主键
  `name` varchar(255) NOT NULL default '',							#域名，完整的域名信息
  `ttl` int(11) NOT NULL default '0',								#TTL时间，仅支持秒级单位
  `rdtype_id` int(11) NOT NULL default '0',							#域名解析对应的类型ID，如 A,NS,SOA等，后续会介绍对应关系 
  `rdata` varchar(255) NOT NULL default '',							#域名解析的值
  `isp_id` int(11) NOT NULL default '0',							#解析所对应的运营商ID
  `location_id` int(11) NOT NULL default '0',						#解析所对应的地域ID
  `idc_id` int(11) NOT NULL default '0',							#解析所对应的Internet机房信息ID
  `flag` tinyint(4) NOT NULL default '0',  							#解析是否可用，可用为1，不可用为0
  `opt_time` int(11) NOT NULL default '0'							#操作时间
  PRIMARY KEY  (`id`)
) DEFAULT CHARSET=utf8;
如果需要与运维相关的一些管理操作，可能需要在解析表中添加一些额外的字段来标准增删改查的信息。

#### 管理表
管理表是用来显示IP表和解析表中的一些外键信息，在一些运维管理系统中能够更容易识别，但这些表在Mysql_BIND中并不会用到。

运营商表
create table isp_tbl (
	id int not null primary key auto_increment,						#运营商ID
	`name` varchar(255) not null DEFAULT ''							#运营商名字，例如：联通，电信等
)DEFAULT CHARSET=utf8;

地域表
create table location_tbl (
	id int not null primary key auto_increment,						#地域ID
	`name` varchar(255) not null DEFAULT ''							#地域名，例如：北京，上海等
)DEFAULT CHARSET=utf8;

Internet机房表
create table idc_tbl (
	id int not null primary key auto_increment,						#机房ID
	`name` varchar(255) not null DEFAULT ''							#机房名称，例如：北京联通机房
)DEFAULT CHARSET=utf8;

Internet机房的相关数据，可以用来在操作域名时更加直观机房的IP对应信息
create table idc_data (
	id int not null primary key auto_increment,						#机房数据ID
	sip int unsigned not null default 0,							#机房网段起始IP
	mask int not null default 0,									#机房网段子网掩码
	idc_id int not null default 0,									#机房ID外键
	isp_id int(11) NOT NULL default '0',							#运营商ID外键
  	location_id int(11) NOT NULL default '0'						#地域ID外键
)DEFAULT CHARSET=utf8;

域名解析所对应的类型表，该表的值是固定的，这对于在一些管理系统中配置类型非常重要
CREATE TABLE `rdtype_tbl` (
  `id` int(11) NOT NULL AUTO_INCREMENT,								#类型表主键
  `rdid` int(11) NOT NULL,											#类型ID，对应IP表和解析表的数据，唯一
  `name` varchar(255) NOT NULL DEFAULT '',							#类型名称，如 A，NS等
  PRIMARY KEY (`id`),
  UNIQUE KEY `rdid` (`rdid`)
) DEFAULT CHARSET=utf8；
详细的表数据可通过下述SQL进行导入
INSERT INTO `rdtype_tbl` VALUES (1,0,'NONE'),(2,1,'A'),(3,2,'NS'),(4,3,'MD'),(5,4,'MF'),(6,5,'CNAME'),(7,6,'SOA'),(8,7,'MB'),(9,8,'MG'),(10,9,'MR'),(11,10,'NULL'),(12,11,'WKS'),(13,12,'PTR'),(14,13,'HINFO'),(15,14,'MINFO'),(16,15,'MX'),(17,16,'TXT'),(18,17,'RP'),(19,18,'AFSDB'),(20,19,'X25'),(21,20,'ISDN'),(22,21,'RT'),(23,22,'NSAP'),(24,23,'NSAP_PTR'),(25,24,'SIG'),(26,25,'KEY'),(27,26,'PX'),(28,27,'GPOS'),(29,28,'AAAA'),(30,29,'LOC'),(31,30,'NXT'),(32,33,'SRV'),(33,35,'NAPTR'),(34,36,'KX'),(35,37,'CERT'),(36,38,'A6'),(37,39,'DNAME'),(38,41,'OPT'),(39,42,'APL'),(40,43,'DS'),(41,44,'SSHFP'),(42,45,'IPSECKEY'),(43,46,'RRSIG'),(44,47,'NSEC'),(45,48,'DNSKEY'),(46,49,'DHCID'),(47,50,'NSEC3'),(48,51,'NSEC3PARAM'),(49,52,'TLSA'),(50,55,'HIP'),(51,59,'CDS'),(52,60,'CDNSKEY'),(53,61,'OPENPGPKEY'),(54,99,'SPF'),(55,103,'UNSPEC'),(56,104,'NID'),(57,105,'L32'),(58,106,'L64'),(59,107,'LP'),(60,108,'EUI48'),(61,109,'EUI64'),(62,249,'TKEY'),(63,250,'TSIG'),(64,256,'URI'),(65,257,'CAA'),(66,32769,'DLV'),(67,65533,'KEYDATA'),(68,251,'IXFR'),(69,252,'AXFR'),(70,253,'MAILB'),(71,254,'MAILA'),(72,255,'ANY');

## 使用实例
以下实例中为了能够使显示更加直观，IP表中的IP地址使用的是字符串的形式显示，涉及到的运营商，地域，Internet机房以及解析类型，都使用数据库关联以后的name进行显示。

### 实例一
当我们希望访问域名a.myexample.com的北京联通用户能够解析到北京联通的机房，而北京电信的用户能够解析到北京电信的机房，我们可以进行如下配置：

IP表 ip_myexample_com 的部分信息如下：
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| sip			  | mask | real_isp  | real_location | isp_id   | location_id| idc_id          |
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| 118.186.208.0   |   20 | 联通      | 北京           | 联通      | 北京       | 北京联通IDC      |
| 219.141.128.0   |   17 | 电信      | 北京           | 电信      | 北京       | 北京电信IDC      |

域名解析表 myexample_com 的部分信息如下：
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| name			     | ttl   | rdtype_id | rdata           | isp_id   | location_id | idc_id          |
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| a.myexample.com    |   900 | A         | 123.125.40.1    | 联通      | 北京        | 北京联通IDC      |
| a.myexample.com    |   900 | A         | 220.181.1.1     | 电信      | 北京        | 北京电信IDC      |

这样就能通过匹配的最高优先级'相同的Internet机房'来匹配成功。

### 实例二
当我们希望北京联通的用户在访问a.myexample.com域名时，解析到北京联通的机房。而访问b.myexample.com域名时，解析到北京BGP机房，我们可以进行如下配置：

IP表 ip_a_myexample_com 的部分信息如下：
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| sip			  | mask | real_isp  | real_location | isp_id   | location_id| idc_id          |
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| 118.186.208.0   |   20 | 联通      | 北京           | 联通      | 北京       | 北京联通IDC      |

IP表 ip_b_myexample_com 的部分信息如下：
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| sip			  | mask | real_isp  | real_location | isp_id   | location_id| idc_id          |
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| 118.186.208.0   |   20 | 联通      | 北京           | 联通      | 北京       | 北京BGP IDC      |

域名解析表 myexample_com 的部分信息如下：
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| name			     | ttl   | rdtype_id | rdata           | isp_id   | location_id | idc_id          |
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| a.myexample.com    |   900 | A         | 123.125.40.1    | 联通      | 北京        | 北京联通IDC      |
| b.myexample.com    |   900 | A         | 220.123.1.1     | BGP      | 北京        | 北京BGP IDC      |

这样就能通过不同的IP表匹配到不同的IDC信息，从而通过最高优先级‘相同的Internet机房’来匹配成功。

### 实例三
当我们希望访问a.myexample.com域名的北京移动用户解析到北京移动的机房，而上海移动的用户解析到上海移动的机房，其他地区的移动用户能够同时解析到北京移动和上海移动两个机房，我们可以进行如下配置：

IP表 ip_myexample_com 的部分信息如下：
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| sip			  | mask | real_isp  | real_location | isp_id   | location_id| idc_id          |
+-----------------+------+-----------+---------------+----------+------------+-----------------+
| 111.150.0.0     |   16 | 移动      | 北京           | 移动      | 北京       | 北京移动IDC      |
| 111.212.0.0     |   17 | 移动      | 上海           | 移动      | 上海       | 上海移动IDC      |
| 183.196.0.0     |   15 | 移动      | 河北           | 移动      | 0         | 0               |
| 183.208.0.0     |   14 | 移动      | 江苏           | 移动      | 0         | 0               |

域名解析表myexample_com 的部分信息如下：
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| name			     | ttl   | rdtype_id | rdata           | isp_id   | location_id | idc_id          |
+--------------------+-------+-----------+-----------------+----------+-------------+-----------------+
| a.myexample.com    |   900 | A         | 111.13.1.1      | 移动      | 北京        | 北京移动IDC      |
| a.myexample.com    |   900 | A         | 112.26.1.1      | 移动      | 上海        | 上海移动IDC      |

这样，北京移动和上海移动的用户可以通过最高优先级‘相同的Internet机房’解析到相应的A记录。而其他地区的移动用户，由于Internet机房和地域两项均设置为0，因此可以通过第三优先级‘相同的运营商’解析到北京和上海两个IDC轮询的A记录上。


## 后续
综上介绍，Mysql_BIND能够在复杂多变的网络环境下提供更加灵活便利的解析方式，能够极大简化运维人员的操作复杂性，缩减时间成本。
当然，Mysql_BIND的优化还需要持续进行，由于定位的复杂性，Mysql_BIND的性能还达不到原生BIND的程度，因此性能的优化道路也会持续进行。
对于DNSSEC等功能的支持，也会持续进行。
另外，还有一些周边的诸如伪造来源IP的测试系统等等，也会在后续逐步开源，希望能够在工作中帮助到大家。


