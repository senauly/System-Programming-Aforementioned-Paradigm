for((i = 0; i < 40; i++))
do
	./client -s path -o data.csv &
done
wait
