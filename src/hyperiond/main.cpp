#include <cassert>
#include <csignal>
#include <unistd.h>
#include <sys/prctl.h> 

#include <QCoreApplication>
#include <QLocale>
#include <QFile>

#include "HyperionConfig.h"

#include <getoptPlusPlus/getoptpp.h>
#include <utils/Logger.h>



#include "hyperiond.h"

using namespace vlofgren;

void signal_handler(const int signum)
{
	QCoreApplication::quit();

	// reset signal handler to default (in case this handler is not capable of stopping)
	signal(signum, SIG_DFL);
}


void startNewHyperion(int parentPid, std::string hyperionFile, std::string configFile)
{
	if ( fork() == 0 )
	{
		sleep(3);
		execl(hyperionFile.c_str(), hyperionFile.c_str(), "--parent", QString::number(parentPid).toStdString().c_str(), configFile.c_str(), NULL);
		exit(0);
	}
}


int main(int argc, char** argv)
{
	Logger* log = Logger::getInstance("MAIN", Logger::INFO);

	// Initialising QCoreApplication
	QCoreApplication app(argc, argv);

	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGCHLD, signal_handler);

	// force the locale
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::c());

	OptionsParser optionParser("Hyperion Daemon");
	ParameterSet & parameters = optionParser.getParameters();

	SwitchParameter<>      & argVersion               = parameters.add<SwitchParameter<>>     (0x0, "version",       "Show version information");
	IntParameter           & argParentPid             = parameters.add<IntParameter>          (0x0, "parent",        "pid of parent hyperiond");
	SwitchParameter<>      & argHelp                  = parameters.add<SwitchParameter<>>     ('h', "help",          "Show this help message and exit");

	argParentPid.setDefault(0);
	optionParser.parse(argc, const_cast<const char **>(argv));
	const std::vector<std::string> configFiles = optionParser.getFiles();

	// check if we need to display the usage. exit if we do.
	if (argHelp.isSet())
	{
		optionParser.usage();
		return 0;
	}

	if (argVersion.isSet())
	{
	std::cout
		<< "Hyperion Ambilight Deamon (" << getpid() << ")" << std::endl
		<< "\tVersion   : " << HYPERION_VERSION_ID << std::endl
		<< "\tBuild Time: " << __DATE__ << " " << __TIME__ << std::endl;

		return 0;
	}

	if (configFiles.size() == 0)
	{
		Error(log, "Missing required configuration file. Usage: hyperiond <options ...> [config.file ...]");
		return 1;
	}

	
	if (argParentPid.getValue() > 0 )
	{
		Info(log, "hyperiond client, parent is pid %d",argParentPid.getValue());
		prctl(PR_SET_PDEATHSIG, SIGHUP);
	}
	
	int argvId = -1;
	for(size_t idx=0; idx < configFiles.size(); idx++) {
		if ( QFile::exists(configFiles[idx].c_str()))
		{
			if (argvId < 0) argvId=idx;
			else startNewHyperion(getpid(), argv[0], configFiles[idx].c_str());
		}
	}
	
	if ( argvId < 0)
	{
		Error(log, "No valid config found");
		return 1;
	}
	
	HyperionDaemon* hyperiond = new HyperionDaemon(configFiles[argvId], &app);
	hyperiond->run();

	// run the application
	int rc = app.exec();
	Info(log, "INFO: Application closed with code %d", rc);

	delete hyperiond;

	return rc;
}
