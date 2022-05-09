#include <sys/types.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

using namespace std;

struct sysinfo memInfo;
static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

void initGetCpu() {
	FILE* file = fopen("/proc/stat", "r");
	fscanf(file, "cpu %llu %llu %llu %llu ", &lastTotalUser, &lastTotalUserLow,
		&lastTotalSys, &lastTotalIdle);
	fclose(file);
}

unsigned long long getMemAvailable() {
	unsigned long long memAvailable;
	FILE* file = fopen("/proc/meminfo", "r");
	fscanf(file, "%*s%*llu kB\n%*s%*llu kB\n%*s%llu kB\n", &memAvailable);
	fclose(file);
	return memAvailable*1024;
}

double getCurrentCpuValue() {
	double percent;
	FILE* file;
	unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

	file = fopen("/proc/stat", "r");
	fscanf(file, "cpu %llu %llu %llu %llu ", &totalUser, &totalUserLow,
		&totalSys, &totalIdle);
	fclose(file);

	if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
			totalSys < lastTotalSys || totalIdle < lastTotalIdle) {
		// Overflow detection. Just skip this value.
		percent = -1.0;
	} else {
		cout << "\tLast:\t" << lastTotalUser << "\t" << lastTotalUserLow << "\t"
					<< lastTotalSys << "\t" << lastTotalIdle << endl;
		cout << "\t\t" << totalUser << "\t" << totalUserLow << "\t"
					<< totalSys << "\t" << totalIdle << endl;
		total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow)
				+ (totalSys - lastTotalSys);
		percent = total;
		cout << "\tTotal 1: " << total << endl;
		total = total + (totalIdle - lastTotalIdle);
		cout << "\tTotal 2: " << total << endl;
		percent /= total;
		cout << "\tPercent: " << percent << endl;
		percent *= 100;
	}

	lastTotalUser = totalUser;
	lastTotalUserLow = totalUserLow;
	lastTotalSys = totalSys;
	lastTotalIdle = totalIdle;

	if (percent == percent) // Check if percent is NaN
		return percent;
	else return -1.0;
}

int main() {
	sysinfo (&memInfo);

	long long totalPhysMem = memInfo.totalram;
	long long usedPhysMem = memInfo.totalram - getMemAvailable();
	
	// Multiply in next statement to avoid int overflow on right hand side
	totalPhysMem *= memInfo.mem_unit;
	usedPhysMem *= memInfo.mem_unit;
	
	cout << "Total RAM: " << (double) totalPhysMem/1024/1000/1000 << " GB" << endl;
	cout << "Currently used RAM: " << (double) usedPhysMem/1024/1000/1000 << " GB" << endl;
	
	// initGetCpu();
	double currentCpu = getCurrentCpuValue();
	cout << "Currently used CPU: " << currentCpu << "%" << endl;
	
	return 0;
}