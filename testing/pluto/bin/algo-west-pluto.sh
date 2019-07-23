#!/bin/sh

set -eu

. ../bin/algo-common.sh

echo check the stack is ${initiator_stack}
grep protostack=${initiator_stack} /etc/ipsec.conf

echo confirm that the network is alive
../bin/wait-until-alive -I 192.0.1.254 192.0.2.254

echo ensure that clear text does not get through
iptables -A INPUT -i eth1 -s 192.0.2.0/24 -j LOGDROP
iptables -I INPUT -m policy --dir in --pol ipsec -j ACCEPT
../../pluto/bin/ping-once.sh --down -I 192.0.1.254 192.0.2.254

# specify the kernel module from the command line?
ipsec start
/testing/pluto/bin/wait-until-pluto-started
ipsec whack --impair suppress-retransmits

echo testing ${algs}
for alg in ${algs} ; do
    name=${proto}-${version}-${alg}
    echo +
    echo + ${name}
    echo +
    ( set -x ; ipsec whack --name ${name} \
		     --${version}-allow \
		     --psk \
		     --esp ${alg} \
		     --${proto} \
		     --pfs \
		     --no-esn \
		     \
		     --id @west \
		     --host 192.1.2.45 \
		     --nexthop 192.1.2.23 \
		     --client 192.0.1.0/24 \
		     \
		     --to \
		     \
		     --id @east \
		     --host 192.1.2.23 \
		     --nexthop=192.1.2.45 \
		     --client 192.0.2.0/24 \
	)
    echo +
    ipsec auto --up ${name}
    echo +
    ../bin/ping-once.sh --up -I 192.0.1.254 192.0.2.254
    echo +
    ipsec auto --delete ${name}
    echo +
done
