# unfortunately does not yet indicate it is using TCP
ipsec auto --up ikev2-westnet-eastnet
ping -n -c4 -I 192.0.1.254 192.0.2.254
ipsec whack --trafficstatus
# should show tcp being used
../../pluto/bin/ipsec-look.sh | grep encap
../../pluto/bin/ipsec-look.sh
ipsec auto --down ikev2-westnet-eastnet
echo "done"
