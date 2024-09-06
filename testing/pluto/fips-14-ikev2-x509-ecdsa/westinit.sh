/testing/guestbin/swan-prep --fips --nokeys
rm /etc/ipsec.d/*db
ipsec initnss > /dev/null 2> /dev/null
ipsec pk12util -i /testing/x509/strongswan/strongWest.p12 -w /testing/x509/nss-pw -k /testing/x509/nss-pw
# Tuomo: why doesn't ipsec checknss --settrust work here?
ipsec certutil -M -n "strongSwan CA - strongSwan" -t CT,,
#ipsec start
ipsec pluto --config /etc/ipsec.conf --leak-detective
../../guestbin/wait-until-pluto-started
ipsec auto --add westnet-eastnet-ikev2
ipsec whack --impair suppress_retransmits
echo "initdone"
