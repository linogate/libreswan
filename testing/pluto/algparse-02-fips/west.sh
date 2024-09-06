../../guestbin/swan-prep --fips --nokeys

# Run the parser tests.

ipsec algparse -tp -fips -v1
ipsec algparse -tp -fips -v1 -pfs
ipsec algparse -tp -fips -v2
ipsec algparse -tp -fips -v2 -pfs

# Run the algorithm tests; there should be no fails.

ipsec algparse -ta > /dev/null

# Check that pluto is starting in the correct mode.

ipsec start
../../guestbin/wait-until-pluto-started
grep ^FIPS /tmp/pluto.log

# Check pluto algorithm list.

sed -n -e '/^|/d' -e ':algs / algorithms:/ { :alg ; p ; n ; /^  / b alg ; b algs }' /tmp/pluto.log
