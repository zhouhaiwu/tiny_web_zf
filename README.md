# tiny_web_zf
一、	HTML真正的强大之处在于一个页面可以包含指针（超链接）；
二、	对于Web客户端和服务器而言，内容是一个MIME类型相关的字节序列；
三、	URL：Universal Resource Locator，通用资源定位符；
四、	HTTP支持许多不同的方法，包括：GET、POST、OPTIONS、HEAD、PUT、DELETE、TRACE；
五、	URI（Uniform Resource Identifier，统一资源标识符）;
六、	代理服务器；
七、	CGI（Common Gateway Interface，通用网关接口）；

****************************************************

一、	现在模拟一个场景，在一个公网里，有A和B两台电脑，分别称为A端和B端，B端里面部署了一台虚拟机，我们称为C端，目前的情况是这样子的，A端和B端网络是通的，B端和C端网络是通的，A端和C端网络是不通的，有一个需求：A端访问C端；
二、	我们实现的方式是这样子的：A端发起请求，通过B端转发给C端，C端处理完，返回数据给B端，B端返回给A端显示；
