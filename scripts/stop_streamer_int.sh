#!/bin/bash

ps faux | grep ElGA | grep streamer | awk '{print $2}' | xargs kill
