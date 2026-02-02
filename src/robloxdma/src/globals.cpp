#include "globals.h"
#include <game/game.h>
void v2::exit(const char* message)
{
	logger->log<error>(message);
	sleep(5000);
}