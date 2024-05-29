/testing/guestbin/swan-prep --x509 --46
dig +short east46.testing.libreswan.org A
dig +short east46.testing.libreswan.org AAAA
../../guestbin/route.sh -4
../../guestbin/route.sh -6
../../guestbin/wait-until-alive 2001:db8:0:2::254
../../guestbin/wait-until-alive 2001:db8:1:2::23
../../guestbin/route.sh get to 2001:db8:1:2::23
# change the default route
../../guestbin/route.sh -6 del default
../../guestbin/ping-once.sh --down 2001:db8:1:2::23
# add default via link local
../../guestbin/route.sh -6 add default dev eth0 via fe80::1000:ff:fe32:64ba
../../guestbin/ping-once.sh --up 2001:db8:1:2::23
ip6tables -A INPUT -i eth0 -s 2001:db8:0:2::254 -p ipv6-icmp -j DROP
ip6tables -I INPUT -m policy --dir in --pol ipsec -j ACCEPT
../../guestbin/ping-once.sh --down 2001:db8:0:2::254
ipsec start
../../guestbin/wait-until-pluto-started
ipsec whack --impair suppress_retransmits
# this test need --verbose to see source address selection
ipsec auto --add --verbose road
echo "initdone"
