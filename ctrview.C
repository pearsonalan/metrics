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

	BOOST_FOREACH(metrics::CounterDefinitionPtr ctrdef, mdef.getCounterDefinitions()) {
		std::cout << ctrdef->getName() << std::endl;
	}

	for (;;) {
		sleep(1);
		metrics::MetricsInstancePtr inst = mdef.getInstance();
		if (inst) {
			std::map<metrics::COUNTERID,metrics::Variant> sample;
			if (inst->sample(sample)) {
				clear();
				int n=0;
				BOOST_FOREACH(metrics::CounterDefinitionPtr ctrdef, mdef.getCounterDefinitions()) {
					mvprintw(n++,0,"%s = %s", ctrdef->getName().c_str(),boost::lexical_cast<std::string>(sample[ctrdef->getId()]).c_str());
				}
				refresh();
			}
		}
	}

	endwin();
}
