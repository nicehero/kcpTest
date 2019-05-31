import os
def do_os(cmd):
	b = os.system(cmd)
	if b != 0:
		exit(1)
do_os('rm -rf include')
do_os('rm -rf lib')

do_os('mkdir include')
do_os('mkdir lib')

print 'download asio'
do_os("wget https://github.com/chriskohlhoff/asio/archive/asio-1-12-2.tar.gz")
do_os("tar xvf asio-1-12-2.tar.gz")
do_os("mv asio-asio-1-12-2/asio/include/asio include/")
do_os("mv asio-asio-1-12-2/asio/include/asio.hpp include/asio/")
do_os("rm -rf asio-asio-1-12-2")
do_os("rm -rf asio-1-12-2.tar.gz")


print 'download kcp'
do_os("wget https://github.com/skywind3000/kcp/archive/1.3.tar.gz")
do_os("tar xvf 1.3.tar.gz")
do_os("mv kcp-1.3 include/kcp")
do_os("rm -rf 1.3.tar.gz")
print 'build kcp'
if os.name == "nt":
	body = open("include/kcp/ikcp.c","rb").read()
	body = body.replace("vsprintf(buffer","vsnprintf(buffer,1024")
	open("include/kcp/ikcp.c","wb").write(body)
	do_os("gcc -c include/kcp/ikcp.c")
	do_os("ar -r lib/ikcp.lib ikcp.o")
else:
	do_os("gcc -shared -fPIC -o lib/libikcp.so include/kcp/ikcp.c")
do_os("rm -rf ikcp.o")

do_os("echo 0.1 > done")
print 'done'