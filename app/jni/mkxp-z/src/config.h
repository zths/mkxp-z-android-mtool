/*
 ** config.h
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "util/json5pp.hpp"

#include <set>
#include <string>
#include <vector>

struct Config {
    // Used for sending the JSON data to Ruby as System::CONFIG
    json5pp::value raw;
    
    int rgssVersion;
    
    bool debugMode;
    bool winConsole;
    bool preferMetalRenderer;
    bool displayFPS;
    bool printFPS;
    
    bool winResizable;
    bool fullscreen;
    bool fixedAspectRatio;
    int smoothScaling;
    int smoothScalingDown;
    int bitmapSmoothScaling;
    int bitmapSmoothScalingDown;
    bool smoothScalingMipmaps;
    int bicubicSharpness;
#ifdef MKXPZ_SSL
    double xbrzScalingFactor;
#endif
    bool enableHires;
    double textureScalingFactor;
    double framebufferScalingFactor;
    double atlasScalingFactor;
    bool vsync;
    
    int defScreenW;
    int defScreenH;
    std::string windowTitle;
    
    int fixedFramerate;
    bool frameSkip;
    bool syncToRefreshrate;
    
    std::vector<std::string> solidFonts;
    
    bool subImageFix;
    bool enableBlitting;
    int maxTextureSize;
    
    struct {
        bool active;
        bool lastMileScaling;
    } integerScaling;
    
    std::string gameFolder;
    bool manualFolderSelect;
    
    bool anyAltToggleFS;
    bool enableReset;
    bool enableSettings;
    bool allowSymlinks;
    bool pathCache;
    
    std::string dataPathOrg;
    std::string dataPathApp;
    
    std::string iconPath;
    std::string execName;
    std::string titleLanguage;
    
    struct {
        std::string soundFont;
        bool chorus;
        bool reverb;
    } midi;
    
    struct {
        int sourceCount;
    } SE;
    
    struct {
        int trackCount;
    } BGM;
    
    bool useScriptNames;
    
    std::string customScript;
    
    std::vector<std::string> launchArgs;
    std::vector<std::string> preloadScripts;
    std::vector<std::string> postloadScripts;
    std::vector<std::string> rtps;
    std::vector<std::string> patches;
    
    std::vector<std::string> fontSubs;
    
    std::vector<std::string> rubyLoadpaths;

    /* Editor flags */
    struct {
        bool debug;
        bool battleTest;
    } editor;
    
    /* Game INI contents */
    struct {
        std::string scripts;
        std::string title;
    } game;
    
    // MJIT Options
    struct {
        bool enabled;
        int verboseLevel;
        int maxCache;
        int minCalls;
    } jit;
    
    // YJIT Options
    struct {
        bool enabled;
    } yjit;

    bool dumpAtlas;

    // Keybinding action name mappings
    struct {
        std::string a;
        std::string b;
        std::string c;
        
        std::string x;
        std::string y;
        std::string z;
        
        std::string l;
        std::string r;
    } kbActionNames;
    
    std::string userConfPath;
    
    /* Internal */
    std::string customDataPath;
    
    Config();
    
    bool fontIsSolid(const char *fontName) const;
    
    void read(int argc, char *argv[]);
    void readGameINI();
};

#endif // CONFIG_H
