./argus -r
sleep 1
./argus -l
sleep 1
./argus -e "cut -f7 -d: /etc/passwd | uniq | wc -l "
sleep 1
./argus -e "date"
sleep 1
./argus -e "ls -l | wc -l"
sleep 1
./argus -e "ps -l"
sleep 1
./argus -e "grep -v ˆ# /etc/passwd | cut -f7 -d: | uniq | wc -l"
sleep 1
./argus -e "ls /etc | wc -l"
sleep 1
./argus -e "date"
sleep 1 
./argus -r
sleep 1
./argus -l
sleep 1
cat log.txt
