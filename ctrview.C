#include <iostream>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <curses.h>

#include "Metrics.H"

int main(int argc, char** argv) {
	if (argc !=2 || strlen(argv[1]) != 4) {
		std::cerr << "Usage: ctrview CTRID" << std::endl;
		return 1;
	}

	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	scrollok(stdscr, TRUE);

	std::string ctrname(argv[1]);
	metrics::METRICSID metricsId = metrics::idFromString<metrics::METRICSID>(ctrname);

	metrics::MetricsDefinition mdef(metricsId);
	do {
		try {
			mdef.initialize();
			break;
		} catch (std::exception& x) {
			clear();
			mvprintw(0,0,"Cannot init: %s",x.what());
			refresh();
		}
		sleep(1);
	} while(1);

	clear();
	refresh();

	metrics::Sample prevSample;
	for (;;) {
		sleep(1);
		metrics::MetricsInstancePtr inst = mdef.getInstance();
		if (inst) {
			metrics::Sample sample;
			if (inst->sample(sample)) {

				sample.format(mdef,prevSample);

				clear();
				int n=2;
				mvprintw(0,0,"SAMPLE @ %lld\n", sample.getTime());
				BOOST_FOREACH(metrics::CounterDefinitionPtr ctrdef, mdef.getCounterDefinitions()) {
					mvprintw(n,0,"[%s.%s]", ctrname.c_str(), ctrdef->getName().c_str());
					mvprintw(n,14,"%s", ctrdef->getDescription().c_str());
					mvprintw(n,50,"%s", boost::lexical_cast<std::string>(sample[ctrdef->getId()]).c_str());
					n++;
				}
				refresh();

				prevSample = sample;
			}
		}
	}

	endwin();
}
