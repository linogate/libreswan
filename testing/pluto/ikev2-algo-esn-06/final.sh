# replay-window will show up as 0 when ESN is enabled due to kernel bug.
ip xfrm state |grep replay-window
: ==== cut ====
ipsec auto --status
ipsec look # ../guestbin/ipsec-look.sh
: ==== tuc ====
