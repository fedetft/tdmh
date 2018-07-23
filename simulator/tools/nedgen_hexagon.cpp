/***************************************************************************
 *   Copyright (C) 2018 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cassert>

using namespace std;

// Printing stuff

void printIni(ostream& os, string name, int simtime)
{
	os<<"[General]\n"
	  <<"network = "<<name<<"\n"
	  <<"eventlog-file = ${resultdir}/"<<name<<".elog\n"
	  <<"experiment-label = "<<name<<"\n"
	  <<"sim-time-limit = "<<simtime<<"s\n"
	  <<"record-eventlog = true\n";
}

void printHeader(ostream& os, string name, int n, int maxn, int h)
{
	os//<<"package wandstemmac.simulations;\n\n"
	  <<"import wandstemmac.Node;\n"
	  <<"import wandstemmac.RootNode;\n\n"
	  <<"network "<<name<<"\n"
	  <<"{\n"
	  <<"    parameters:\n"
	  <<"        n*.nodes = "<<maxn<<";\n"
	  <<"        n*.hops = "<<h<<";\n"
	  <<"    submodules:\n"
	  <<"        n0: RootNode {\n"
	  <<"            address = 0;\n"
	  <<"        }\n";
	for(int i=1;i<n;i++)
	{
		os<<"        n"<<i<<": Node {\n"
		  <<"            address = "<<i<<";\n"
		  <<"        }\n";
	}
	os<<"    connections:\n";
}

void printFooter(ostream& os)
{
	os<<"}\n";
}

void print(ostream& os, int id1, int id2)
{
	os<<"        n"<<id1<<".wireless++ <--> n"<<id2<<".wireless++;\n";
}

// Point and helpers

struct Point
{
	Point(double x, double y, int id=-1) : x(x), y(y), id(id) {}
	double x,y;
	int id;
};

double distance(Point a, Point b)
{
	return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

bool operator==(Point a, Point b)
{
	return distance(a,b)<0.05f;
}

Point move(Point p, int direction, int id=-1)
{
	//Move in a heagon
	//0=right, 1=lower right, 2=lower left, 3=left, 4=upper left 5=upper right
	assert(direction>=0 && direction<6);
	double radius=1;
	return Point(p.x+radius*cos(-M_PI/3*direction),
				 p.y+radius*sin(-M_PI/3*direction),id);
}

// Hexagon field algorithm

void generator(function<void (const vector<Point>&, Point)> callback, int n, bool reverse)
{
	if(n<=0) return;
	vector<Point> points;
	
	auto add=[&](Point p) { //Returns if the callback signaled to stop the algorithm
		callback(points,p);
		points.push_back(p);
	};
	
	auto exists=[&](Point p)->bool {
		return find(points.begin(),points.end(),p)!=points.end();
	};
	
	Point p(0,0,0);
	add(p); //Add the first point
	int id=reverse ? n-1 : 1;
	int count=0;
	if(++count>=n) return;
	for(int radius=1;;radius++)
	{
		p=move(p,0,reverse ? id-- : id++); //Gone full circle, move right
		add(p);
		if(++count>=n) return;
		int direction=2; //Start by moving down, left
		do {
			for(int i=0;i<radius;i++)
			{
				p=move(p,direction,id);
				if(exists(p)) continue; //Happens the last time in a circle
				add(p);
				reverse ? id-- : id++;
				if(++count>=n) return;
			}
			direction=(direction+1)%6;
		} while(direction!=2);
	}
}

int main(int argc, char *argv[])
{
	if(argc!=7)
	{
		cerr<<"use: ./nedgen filename n maxn h simtime reverse"<<endl;
		return 1;
	}
	string filename=argv[1];
	int n=stoi(argv[2]);
	int maxn=stoi(argv[3]);
	int h=stoi(argv[4]);
	int simtime=stoi(argv[5]);
	bool reverse=stoi(argv[6]);
	
	ofstream ini((filename+".ini").c_str());
	printIni(ini,filename,simtime);
	ofstream ned((filename+".ned").c_str());
	printHeader(ned,filename,n,maxn,h);
	
	generator([&](const vector<Point>& points, Point p) {
		cout<<p.x<<" "<<p.y<<" "<<p.id<<endl;
		
		for(int i=0;i<6;i++)
		{
			Point q=move(p,i);
			auto it=find(points.begin(),points.end(),q);
			if(it==points.end()) continue;
			print(ned,p.id,it->id);
		}
	},n,reverse);
	printFooter(ned);
}
