#include <iostream>

#include <stdio.h>
#include <signal.h>
#include <curses.h>

#include "Metrics.H"

void sigcatch(int sig)
{
	endwin();
	exit(1);
}

void setupSignalHandlers()
{
	signal(SIGINT,sigcatch);
	signal(SIGQUIT,sigcatch);
	signal(SIGTERM,sigcatch);
}

metrics::MetricsDefinition mdef('keys');

metrics::MetricsInstancePtr initCounters()
{
	mdef.defineCounter('chra',metrics::COUNTER_TYPE_32BIT);
	mdef.defineCounter('chrb',metrics::COUNTER_TYPE_32BIT);
	mdef.defineCounter('chrc',metrics::COUNTER_TYPE_32BIT);
	mdef.defineCounter('vowl',metrics::COUNTER_TYPE_32BIT);
	mdef.defineCounter('ccnt',metrics::COUNTER_TYPE_32BIT);
	mdef.initialize();
	return mdef.getInstance();
}

int main()
{
	try {
		metrics::MetricsInstancePtr inst = initCounters();

		metrics::IntCounterPtr aCounter = inst->getIntCounterById('chra');
		metrics::IntCounterPtr bCounter = inst->getIntCounterById('chrb');
		metrics::IntCounterPtr cCounter = inst->getIntCounterById('chrc');
		metrics::IntCounterPtr vowelCounter = inst->getIntCounterById('vowl');
		metrics::IntCounterPtr charCounter = inst->getIntCounterById('ccnt');

		initscr();
		cbreak();
		noecho();
		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);
		scrollok(stdscr, TRUE);

		int rows, cols, c, n=0;
		getmaxyx(stdscr,rows,cols);
		while ((c = wgetch(stdscr)) != ERR) {
			// exit on DEL or ESC
			if(c == 0x7F || c == 0x1B)
				break;

			if (c == 'a' || c == 'A') {
				aCounter->increment();
			}
			if (c == 'b' || c == 'B') {
				bCounter->increment();
			}
			if (c == 'c' || c == 'C') {
				cCounter->increment();
			}

			if (c == 'a' || c == 'A' || c == 'e' || c == 'E' || c == 'i' || c == 'I' || c == 'o' || c == 'O' || c == 'u' || c == 'U') {
				vowelCounter->increment();
			}

			charCounter->increment();
			mvprintw(std::min(n,rows-1),0,"%x", c);
			n++;
			if (n >= rows) {
				scroll(stdscr);
			}
			mvprintw(std::min(n,rows-1),0,"");
			refresh();
		}
	} catch (std::exception& x) {
		std::cerr << "Exception: " << x.what() << std::endl;
	}

	endwin();
}
