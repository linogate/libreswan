/testing/guestbin/swan-prep --nokeys
../../guestbin/ip.sh link set ipsec2 down 2>/dev/null && ../../guestbin/ip.sh link del ipsec2 2>/dev/null
../../guestbin/ip.sh link set ipsec3 down 2>/dev/null && ../../guestbin/ip.sh link del ipsec3 2>/dev/null
ipsec start
../../guestbin/wait-until-pluto-started
ipsec auto --add north-east
ipsec auto --add north-west
echo "initdone"
