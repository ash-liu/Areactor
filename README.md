Areactor
==================

### Areactor是啥
一个reactor的demo，copy redis的实现，练习用。

redis的源码太赞了！设计的可重用性非常高，而且几乎无外部依赖，所以可以很方便的借鉴到自己的项目中，这个练习的作用
除了是熟悉reactor，IO多路复用等等外，关键还是熟悉redis的源码，KV数据库啥的我是外行，但至少我了解KV数据库的实现啊，
是吧？足够了:)


### 操作环境
宿主是Mac环境，但用惯了source insight，所以源码还是在虚拟机xp中编(chao)辑(xi)，然后用copy命令全部复制到宿主机目录中，
编译&调试也是在Mac环境下，所以使用了kqueue作为IO多路复用。makefile是网上抓的一个通用版本。版本管理使用github for Mac，
直接sync到这里。

...

