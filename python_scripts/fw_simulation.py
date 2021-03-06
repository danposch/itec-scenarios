#! /usr/bin/python

import time
import numpy
import os
import glob
import collections
import shutil
import re
import subprocess
from subprocess import call
import threading
import time
import operator
import consumer_stats as consumer_stats

curActiveThreads = 0
invalid_runs = 0

class Thread(threading.Thread):
    # init
  def __init__(self,job_number, sys_cal, callback_method, src ,dst):
		super(Thread,self).__init__()
		self.sysCall = sys_cal
		self.jobNumber = job_number
		self.callback = callback_method
		self.src = src
		self.dst = dst

  # overwriting run method of threading.Thread (do not call this method, call thread.start() )
  def run(self):

		if not os.path.exists(self.src+"/traces"):
			os.makedirs(self.src+"/traces")

		fpOut = open("t_" + str(self.jobNumber) + ".stdout.txt", "w")

		# start subprocess
		proc = subprocess.Popen(self.sysCall,stdout=fpOut, cwd=self.src)
		proc.communicate() # wait until finished

		# sleep 0.5 seconds to be sure the OS really has finished the process
		time.sleep(0.5)

		fpOut.close()
		os.remove("t_" + str(self.jobNumber) + ".stdout.txt")

		# callback
		print "threadFinished(job_" + str(self.jobNumber) + ")"
		self.callback(self.jobNumber,self.src,self.dst)

def threadFinished(job_number,src,dst):
	#compute statistics

	global curActiveThreads, invalid_runs

	print "generateStatsPerSimulation(job_" + str(job_number) + ")"
	try:
		consumer_stats.generateStatsPerSimulation(src);
	except Exception:
		invalid_runs += 1
		pass

	#copy results
	files = glob.glob(src + "/traces/*STATS*.txt");

	if not os.path.exists(dst):
		os.makedirs(dst)

	for f in files:
		shutil.move(f, dst+"/"+os.path.basename(f))

	#print "DELTE FOLDER " + src
	shutil.rmtree(src)

	print "statsCollected(job_" + str(job_number) + ")"

	curActiveThreads -= 1

def	order_results(path):
	results = {}

	for root, dirs, files in os.walk(path):
		for subdir in dirs:
		
			if "output_run" in subdir:
				continue

			#print root+subdir

			files = glob.glob(root+subdir + "/*/*STATS*.txt" );
		
			avg_ratio = 0.0
			file_count = 0		

			for file in files:

				#print file
				f = open(file, "r")
				for line in f:
					if(line.startswith("Ratio:")):
						avg_ratio += float(line[len("Ratio:"):])
						file_count +=1
						break;

			if(file_count > 0):
	 			avg_ratio /= file_count
	
			#print avg_ratio
			results.update({"AVG_RATIO:"+ subdir : avg_ratio})

	sorted_results = reversed(sorted(results.items(), key=operator.itemgetter(1)))
	f = open(path + "/result.txt", "w")
	for entry in sorted_results:
		f.write(entry[0] + ":" + str(entry[1]) + "\n")
		
def getScenarioName(config,connectivity,strategy,route, linkfailure):
	
	name = ""

	if("fw-strategy=bestRoute" in strategy):
		name += "BestRoute"
	elif("fw-strategy=smartflooding" in strategy):
		name += "SmartFlooding"
	elif("fw-strategy=perContentBased" in strategy):
		name += "PerContentBased"
	elif("fw-strategy=broadcast" in strategy):
		name += "Broadcast"
	else:
		name += "UnknownStrategy"

	if("route=single" in route):
		name += "_SingleRoute"
	elif("route=all" in route):
		name += "_AllRoutes"
	else:
		name += "_UnkownRoute"

	if("low_bw" in config):
		name += "_LowBW"
	elif("medium_bw" in config):
		name += "_MediumBW"
	elif("high_bw" in config):
		name += "_HighBW"
	else:
		name +="_UnkownBriteConfig"

	if("connectivity=low" in connectivity):
		name += "_LowConnectivity"
	elif("connectivity=medium" in connectivity):
		name += "_MediumConnectivity"
	elif("connectivity=high" in connectivity):
		name += "_HighConnectivity"
	else:
		name +="_UnkownConnectivity"


	failure = re.findall('\d+', linkfailure)

	if(len(failure) == 1):
		name += "_LinkFailure_"+failure[0]
	else:
		name +="_UnkownLinkFailurey"

	return name
	

###NOTE Start this script FROM itec-scenarios MAIN-FOLDER!!!

SIMULATION_DIR=os.getcwd()

THREADS = 22
SIMULATION_RUNS = 100
SIMULATION_OUTPUT = SIMULATION_DIR + "/output/"

#brite config file
scenario="brite_example"


#britePath="--briteConfFile=/local/users/ndnsim/ndnSIM/itec-scenarios/"
britePath="/local/users/ndnsim/ndnSIM/itec-scenarios/"

briteConfigLowBw="--briteConfFile="+britePath+"brite_configs/brite_low_bw.conf"
briteConfigMediumBw="--briteConfFile="+britePath+"brite_configs/brite_medium_bw.conf"
briteConfigHighBw="--briteConfFile="+britePath+"brite_configs/brite_high_bw.conf"

briteConfigs = [briteConfigLowBw, briteConfigMediumBw, briteConfigHighBw]
#briteConfigs = [briteConfigMediumBw]

lowConnectivity="--connectivity=low"
mediumConnectivity="--connectivity=medium"
highConnectivity="--connectivity=high"

connectivities = [lowConnectivity, mediumConnectivity, highConnectivity]
#connectivities = [mediumConnectivity]

bestRoute="--fw-strategy=bestRoute"
smartFlooding="--fw-strategy=smartflooding"
broadcast="--fw-strategy=broadcast"
perContentBased="--fw-strategy=perContentBased"

forwardingStrategies = [bestRoute, smartFlooding, perContentBased, broadcast]
#forwardingStrategies = [perContentBased]

singleRoute="--route=single"
allRoute="--route=all"

#routes = [singleRoute]#, allRoute]
routes = [allRoute]

#linkFailures = ["--linkFailures=0", "--linkFailures=15", "--linkFailures=30", "--linkFailures=50", "--linkFailures=100"]
linkFailures = ["--linkFailures=30", "--linkFailures=50", "--linkFailures=100"]

SCENARIOS = {
#"BestRoute_AllRoutes":     { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": [briteConfig, bestRoute, allRoutes] },
#"SmartFlooding_AllRoutes": { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": [briteConfig, smartFlooding, allRoutes] },
#"PerContentBased_AllRoutes": { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": [briteConfig, perContentBased, allRoutes] },

#PerContentBased
#"PerContentBased_SingleRoute_Low_Bw_Low_Connectivity":     { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": #[briteConfigLowBw, perContentBased, singleRoute, lowConnectivity] },
#"PerContentBased_SingleRoute_Low_Bw_Medium_Connectivity": { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": #[briteConfigLowBw, perContentBased, singleRoute, mediumConnectivity] },
#"PerContentBased_SingleRoute_Low_Bw_High_Connectivity": { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": [briteConfigLowBw, perContentBased, singleRoute, highConnectivity] },
}

settings_counter = 0
for config in briteConfigs:
	for connectivity in connectivities:
		for strategy in forwardingStrategies:
			for route in routes:
				for failures in linkFailures:
					name = getScenarioName(config,connectivity,strategy,route, failures) 
					SCENARIOS.update({name : { "executeable": scenario, "numRuns": SIMULATION_RUNS, "params": [config, connectivity, strategy, route, failures] }})			
					settings_counter += 1

#build project before
call([SIMULATION_DIR + "/waf"])

###script start
print "\nCurring working dir = " + SIMULATION_DIR + "\n"

print str(settings_counter) + " different settings selected"
print str(SIMULATION_RUNS) + " runs per setting"
print "Total runs: " + str(settings_counter*SIMULATION_RUNS)

time.sleep(5)

###script start
print "\nCurring working dir = " + SIMULATION_DIR + "\n"

job_number = 0

for scenarioName in SCENARIOS.keys():
	runs = SCENARIOS[scenarioName]['numRuns']
	executeable = SCENARIOS[scenarioName]['executeable']
	
	executeable = "build/" + executeable
	print "------------------------------------------------------------------------"
	print "Starting", runs , "simulations of", scenarioName

  #REMOVE JUST FOR SMALL TEST
  #if(job_number>4):
    #continue
	
	for i in range(0, runs):
  	# See if we are using all available threads
		while curActiveThreads >= THREADS:
			time.sleep(1) # wait 1 second

		print "----------"
		print "Simulation run " + str(i) + " in progress..." 
		sysCall = [SIMULATION_DIR+"/" + executeable] +  SCENARIOS[scenarioName]['params'] + ["--RngRun=" + str(i)] + ["--outputFolder=traces"] ## working folder of subprocess is determined by Thread
		print sysCall

		dst = SIMULATION_OUTPUT+scenarioName + "/output_run"+str(i)
		src = SIMULATION_OUTPUT+"../ramdisk/tmp_folder_" + str(job_number)

	   # start thread, get callback method to be called when thread is done
		thread = Thread(job_number, sysCall, threadFinished, src, dst)
		if(job_number<THREADS): # do this to avoid that all threads copy at the same time
			time.sleep(15.0)
		thread.start()

		job_number += 1
		curActiveThreads += 1	
# end for

while curActiveThreads != 0:
    time.sleep(10)
    print "Active Threads: " + str(curActiveThreads)

order_results(SIMULATION_OUTPUT)

print ""
print "We had " + str(invalid_runs) + " invalid runs"
print "Finished."
