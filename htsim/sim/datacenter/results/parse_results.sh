for speed in 200 400 800 ; do
    for topo in "" _4_to_1 _8_to_1 ; do
	algorithm=rccc
	filename=$speed"g"_$algorithm$topo.txt

	cat $filename | awk '{if ($1=="Experiment:"){ paths = $7;size = $9;} if ($3=="FCT") print size,"\t",paths,"EVs\t",int($4),int($10),int($10/1.2),$4*1.2/$10;}' > a

	algorithm=nscc
	filename=$speed"g"_$algorithm$topo.txt

	cat $filename | awk '{if ($1=="Experiment:"){ paths = $7;size = $9;} if ($3=="FCT") print size,"\t",paths,"EVs\t",int($4),int($10),int($10/1.2),$4*1.2/$10;}' > b	 

	echo $speed"g" $topo " "
	paste a b | grep "32 EVs" | awk '{if ($1!=$8 || $2 != $9) print "Error on this line:",$0; else {print $0;rcccsum += $7;nsccsum+=$14;cnt++;}} END{printf ("RCCC %f NSCC: %f\n",rcccsum/cnt,nsccsum/cnt);}'
	paste a b | grep "64 EVs" | awk '{if ($1!=$8 || $2 != $9) print "Error on this line:",$0; else {print $0;rcccsum += $7;nsccsum+=$14;cnt++;}} END{printf ("RCCC %f NSCC: %f\n",rcccsum/cnt,nsccsum/cnt);}'	
	paste a b | grep "128 EVs" | awk '{if ($1!=$8 || $2 != $9) print "Error on this line:",$0; else {print $0;rcccsum += $7;nsccsum+=$14;cnt++;}} END{printf ("RCCC %f NSCC: %f\n",rcccsum/cnt,nsccsum/cnt);}'

	paste a b | grep "32 EVs" > 32.out
	paste a b | grep "64 EVs" > 64.out
	paste a b | grep "128 EVs" > 128.out

	outfile=$speed"g"$topo.png
	
	echo "set term png" > tt.plot
	echo "set output \""$outfile"\"" >> tt.plot

	oversub=`echo $topo | tr "_" " " | awk '{if ($1>1) print $1; else print 1;}'`

	echo "set title \""$speed"g oversubscribed "$oversub" 8K permutation\"" >> tt.plot
	
	echo "oversub="$oversub >> tt.plot
	echo "speed="$speed >> tt.plot

	cat t.plot >> tt.plot

	#cat tt.plot
	gnuplot tt.plot
	echo "Generating plot in file:"$outfile
    done
done

rm a b tt.plot 32.out 64.out 128.out
