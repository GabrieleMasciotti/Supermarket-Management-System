#!/bin/bash

LOGFILE=$(ls -t *.log -1 | head -1)		#LOGFILE is the name of the latest modified log file in the folder

echo -e "\nStatistiche d'esecuzione prodotte:\n"

while read log_line; do
	echo $log_line;
done < $LOGFILE;


