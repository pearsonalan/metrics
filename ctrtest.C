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
	mdef.defineCounter('chra',"Number of A Keys", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('chrb',"Number of B Keys", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('chrc',"Number of C Keys", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('vowl',"Vowel Keys Pressed", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('pvwl',"Pct. Vowel Keys", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_RATIO | metrics::COUNTER_FLAG_USEPRIORVALUE | metrics::COUNTER_FLAG_PCT, 'kcnt');
	mdef.defineCounter('dvwl',"Delta Vowel Keys Pressed", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_DELTA, 'vowl');
	mdef.defineCounter('vwlr',"Vowel Keys Pressed /sec", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_RATE, 'vowl');
	mdef.defineCounter('kcnt',"Keys Pressed", metrics::COUNTER_TYPE_32BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('keyr',"Keys Pressed /sec", metrics::COUNTER_TYPE_32BIT  | metrics::COUNTER_FORMAT_RATE, 'kcnt');
	mdef.defineCounter('keya',"Keys Pressed /sec /sec", metrics::COUNTER_TYPE_32BIT  | metrics::COUNTER_FORMAT_RATE, 'keyr');
	mdef.defineCounter('ptim',"Print Time", metrics::COUNTER_TYPE_64BIT | metrics::COUNTER_FORMAT_COUNT);
	mdef.defineCounter('ptmd',"Delta Print Time", metrics::COUNTER_TYPE_64BIT | metrics::COUNTER_FORMAT_DELTA, 'ptim');
	mdef.defineCounter('ptmr',"Pct Print Time", metrics::COUNTER_TYPE_64BIT | metrics::COUNTER_FORMAT_TIMER, 'ptim');
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
		metrics::IntCounterPtr charCounter = inst->getIntCounterById('kcnt');
		metrics::LargeCounterPtr printTimeCounter = inst->getLargeCounterById('ptim');

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

			metrics::ScopeTimer timer(printTimeCounter);
			mvprintw(std::min(n,rows-1),0,"0x%x (%c)", c, isprint(c) ? c : ' ');
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
