for ((i=0; i<100; i++))
do
poolsim --config config.json --seed $i
mv results.json results/results_$i.json
done