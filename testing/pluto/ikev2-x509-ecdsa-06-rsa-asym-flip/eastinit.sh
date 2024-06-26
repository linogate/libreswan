/testing/guestbin/swan-prep --x509
ipsec pk12util -i /testing/x509/strongswan/strongEast.p12 -w /testing/x509/nss-pw
# Tuomo: why doesn't ipsec checknss --settrust work here?
ipsec certutil -M -n "strongSwan CA - strongSwan" -t CT,,
#ipsec start
ipsec pluto --config /etc/ipsec.conf --leak-detective
../../guestbin/wait-until-pluto-started
ipsec auto --add westnet-eastnet-ikev2
ipsec status | grep "our auth:"
ipsec whack --impair suppress_retransmits
echo "initdone"
