/testing/guestbin/swan-prep --hostkeys
ipsec start
../../guestbin/wait-until-pluto-started
ipsec auto --add westnet-eastnet-ipcomp
ipsec auto --status | grep westnet-eastnet-ipcomp
echo "initdone"
