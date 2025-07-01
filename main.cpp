#include "raylib.h"
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <memory>
#include <algorithm> // For std::clamp
#include <fstream>
#include <iostream>
#include <unistd.h> // For readlink
#include <libgen.h> // For dirname
#include <cstdlib> // For getenv

// Game States
enum class GameState {
    MENU,
    PLAYING,
    SETTINGS,
    PAUSED
};

// Input modes
enum class InputMode {
    KEYBOARD_MOUSE,
    CONTROLLER
};

// Save data structure
struct SaveData {
    Vector2 playerPos; // This will now store relative coordinates (0.0-1.0)
    bool isFullscreen;
    int targetFPS;
    InputMode inputMode;
    float volume; // 0.0 to 1.0
    
    SaveData() : playerPos{0.1f, 0.1f}, isFullscreen(true), targetFPS(120), inputMode(InputMode::KEYBOARD_MOUSE), volume(0.5f) {}
};

// Menu Item structure
struct MenuItem {
    std::string text;
    Rectangle bounds;
    Color color;
    Color hoverColor;
    bool isHovered;
    bool isSelected; // For controller navigation
    
    MenuItem(const std::string& t, float x, float y, float width, float height) 
        : text(t), bounds{x, y, width, height}, color{DARKGRAY}, hoverColor{BLUE}, isHovered(false), isSelected(false) {}
};

// Game class to manage all game logic
class Game {
private:
    GameState currentState;
    int screenWidth;
    int screenHeight;
    bool isFullscreen;
    int targetFPS;
    bool shouldExit = false;
    bool forceMenuRecalc = false;
    int lastWindowWidth = 0;
    int lastWindowHeight = 0;
    InputMode currentInputMode;
    int selectedMenuItem = 0;
    
    // Menu items
    std::vector<MenuItem> mainMenuItems;
    std::vector<MenuItem> settingsMenuItems;
    std::vector<MenuItem> pauseMenuItems;
    
    // Game variables
    Vector2 playerPos;
    
    // Save data
    SaveData saveData;
    std::string saveFilePath;
    
    // Popup for save
    bool showSavePopup = false;
    float savePopupTimer = 0.0f;
    const float savePopupDuration = 2.0f; // seconds
    
    // SDL2 controller
    SDL_GameController* controller = nullptr;
    bool showControllerDebug = false;
    
    // Button press tracking for menu navigation
    bool dPadUpPressed = false;
    bool dPadDownPressed = false;
    bool dPadLeftPressed = false;
    bool dPadRightPressed = false;
    bool aButtonPressed = false;
    bool bButtonPressed = false;
    bool startButtonPressed = false;
    bool backButtonPressed = false;
    
    // Analog stick menu navigation
    float analogStickThreshold = 0.5f;
    bool analogUpPressed = false;
    bool analogDownPressed = false;
    bool analogLeftPressed = false;
    bool analogRightPressed = false;
    
    // Track when keyboard/controller navigation was last used
    bool keyboardControllerNavigationUsed = false;
    
    // Wayland detection
    bool isWayland = false;
    
    bool pendingFullscreenResize = false;
    int fullscreenResizeFrames = 0;
    
    float volume = 0.5f;
    
    Sound volumeChangeSound;
    bool soundLoaded = false;
    
    void DetectDisplayServer() {
        const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
        const char* x11Display = getenv("DISPLAY");
        
        if (waylandDisplay && strlen(waylandDisplay) > 0) {
            isWayland = true;
            std::cout << "[INFO] Detected Wayland display server" << std::endl;
            
            // Some helpful hints for Wayland users (but don't interfere with SDL)
            std::cout << "[INFO] Wayland detected - if you experience issues:" << std::endl;
            std::cout << "[INFO] - Try running with: GDK_BACKEND=x11 ./game" << std::endl;
            std::cout << "[INFO] - Or: SDL_VIDEODRIVER=x11 ./game" << std::endl;
        } else if (x11Display && strlen(x11Display) > 0) {
            isWayland = false;
            std::cout << "[INFO] Detected X11 display server" << std::endl;
        } else {
            std::cout << "[WARNING] Could not detect display server" << std::endl;
        }
    }
    
    void InitSaveFilePath() {
        char exePath[4096];
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len != -1) {
            exePath[len] = '\0';
            std::string exeDir = dirname(exePath);
            saveFilePath = exeDir + "/game_save.dat";
        } else {
            saveFilePath = "game_save.dat";
        }
    }
    
    void InitSDL2Controller() {
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
            printf("SDL2 could not initialize! SDL_Error: %s\n", SDL_GetError());
            return;
        }
        
        // Check for game controllers
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                controller = SDL_GameControllerOpen(i);
                if (controller) {
                    printf("SDL2 Controller connected: %s\n", SDL_GameControllerName(controller));
                    break;
                }
            }
        }
    }
    
    // Helper function to check if a button was just pressed (not held)
    bool IsButtonJustPressed(bool& buttonState, bool currentState) {
        if (currentState && !buttonState) {
            buttonState = true;
            return true;
        } else if (!currentState) {
            buttonState = false;
        }
        return false;
    }
    
    // Helper function to check if analog stick direction was just pressed
    bool IsAnalogDirectionJustPressed(bool& directionState, float axisValue, float threshold) {
        bool currentState = abs(axisValue) > threshold;
        if (currentState && !directionState) {
            directionState = true;
            return true;
        } else if (!currentState) {
            directionState = false;
        }
        return false;
    }
    
public:
    Game() : currentState(GameState::MENU), screenWidth(1920), screenHeight(1080), 
             isFullscreen(true), targetFPS(120), playerPos{100, 100},
             currentInputMode(InputMode::KEYBOARD_MOUSE), volume(0.5f) {
        InitAudioDevice();
        DetectDisplayServer();
        InitSaveFilePath();
        LoadGame();
        SetMasterVolume(volume);
        // Try to load a default sound effect
        if (FileExists("resources/click.wav")) {
            volumeChangeSound = LoadSound("resources/click.wav");
            soundLoaded = true;
        } else {
            soundLoaded = false;
        }
        InitializeWindow();
        SetPlayerPositionFromSave();
        InitializeMenus();
        InitSDL2Controller();
    }
    
    ~Game() {
        SaveGame();
        if (soundLoaded) UnloadSound(volumeChangeSound);
        if (controller) {
            SDL_GameControllerClose(controller);
        }
        CloseAudioDevice();
        SDL_Quit();
    }
    
    void Run() {
        while (!WindowShouldClose() && !shouldExit) {
            Update();
            Draw();
        }
    }
    
private:
    void InitializeWindow() {
        int monitor = GetCurrentMonitor();
        if (isFullscreen) {
            screenWidth = GetMonitorWidth(monitor);
            screenHeight = GetMonitorHeight(monitor);
        } else {
            screenWidth = 1280;
            screenHeight = 720;
        }
        InitWindow(screenWidth, screenHeight, "2D Game Template");
        SetTargetFPS(targetFPS);
        SetExitKey(KEY_NULL);
        if (isFullscreen) {
            SetWindowState(FLAG_FULLSCREEN_MODE);
            // Force resize to monitor resolution after entering fullscreen
            int monitor = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        }
    }
    
    void SaveGame() {
        // Convert absolute position to relative (0.0-1.0)
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        float playerSize = screenW * 0.03f; // Same as in DrawGame()
        
        // Calculate relative position as pure percentage of window size
        float relativeX = playerPos.x / screenW;
        float relativeY = playerPos.y / screenH;
        
        // Clamp to valid range (0.0 to 1.0)
        relativeX = std::clamp(relativeX, 0.0f, 1.0f);
        relativeY = std::clamp(relativeY, 0.0f, 1.0f);
        
        saveData.playerPos = {relativeX, relativeY};
        saveData.isFullscreen = isFullscreen;
        saveData.targetFPS = targetFPS;
        saveData.inputMode = currentInputMode;
        saveData.volume = volume;
        
        std::ofstream file(saveFilePath, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&saveData), sizeof(SaveData));
            file.close();
            printf("[SAVE] Position: (%.2f, %.2f), Fullscreen: %s\n", 
                   relativeX, relativeY, isFullscreen ? "true" : "false");
            showSavePopup = true;
            savePopupTimer = savePopupDuration;
        } else {
            printf("[ERROR] Failed to save game\n");
        }
        
        if (soundLoaded) PlaySound(volumeChangeSound);
    }
    
    void LoadGame() {
        std::ifstream file(saveFilePath, std::ios::binary);
        if (file.is_open()) {
            file.read(reinterpret_cast<char*>(&saveData), sizeof(SaveData));
            file.close();
            
            // Load settings first
            isFullscreen = saveData.isFullscreen;
            targetFPS = saveData.targetFPS;
            currentInputMode = saveData.inputMode;
            volume = saveData.volume;
        } else {
            printf("No save file found, using defaults\n");
            volume = 0.5f;
        }
    }
    
    void SetPlayerPositionFromSave() {
        // Convert relative position back to absolute
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        float playerSize = screenW * 0.03f;
        
        // Calculate absolute position from pure percentage
        playerPos.x = saveData.playerPos.x * screenW;
        playerPos.y = saveData.playerPos.y * screenH;
        
        // Clamp to screen bounds to keep player fully visible
        playerPos.x = std::clamp(playerPos.x, 0.0f, (float)(screenW - playerSize));
        playerPos.y = std::clamp(playerPos.y, 0.0f, (float)(screenH - playerSize));
    }
    
    void CheckInputMode() {
        InputMode previousInputMode = currentInputMode;
        
        // Check for SDL2 controller input
        if (controller) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_CONTROLLERAXISMOTION) {
                    currentInputMode = InputMode::CONTROLLER;
                }
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    currentInputMode = InputMode::CONTROLLER;
                }
            }
        }
        
        // Check for keyboard/mouse input
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || 
            IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || GetMouseDelta().x != 0 || GetMouseDelta().y != 0 ||
            GetKeyPressed() != 0) {
            currentInputMode = InputMode::KEYBOARD_MOUSE;
        }
        
        // Handle cursor visibility when input mode changes
        if (previousInputMode != currentInputMode) {
            if (currentInputMode == InputMode::CONTROLLER) {
                HideCursor();
            } else {
                ShowCursor();
            }
            
            // Reset keyboard navigation flag when switching to keyboard/mouse
            if (currentInputMode == InputMode::KEYBOARD_MOUSE) {
                keyboardControllerNavigationUsed = false;
            }
        }
        
        // Handle cursor visibility based on actual input activity
        if (currentInputMode == InputMode::KEYBOARD_MOUSE) {
            // Check if mouse is actually moving
            Vector2 mouseDelta = GetMouseDelta();
            bool mouseMoving = (mouseDelta.x != 0 || mouseDelta.y != 0);
            
            // Show cursor if mouse is moving, hide if using keyboard navigation
            if (mouseMoving) {
                ShowCursor();
                keyboardControllerNavigationUsed = false; // Reset flag when mouse moves
            } else if (keyboardControllerNavigationUsed) {
                HideCursor();
            }
        }
    }
    
    void InitializeMenus() {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        
        // Scale button size relative to window size
        float buttonWidth = winW * 0.2f;  // 20% of window width
        float buttonHeight = winH * 0.06f; // 6% of window height
        float buttonSpacing = winH * 0.02f; // 2% of window height
        
        int numMainButtons = 4; // Added Save Game
        int numSettingsButtons = 3; // Volume, Toggle Fullscreen, Back to Menu
        int numPauseButtons = 3; // Added Save Game
        
        float totalMainHeight = numMainButtons * buttonHeight + (numMainButtons - 1) * buttonSpacing;
        float mainStartY = winH / 2.0f - totalMainHeight / 2.0f;
        float centerX = winW / 2.0f - buttonWidth / 2.0f;
        
        mainMenuItems.clear();
        mainMenuItems.emplace_back("Start Game", centerX, mainStartY + 0 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        mainMenuItems.emplace_back("Settings", centerX, mainStartY + 1 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        mainMenuItems.emplace_back("Save Game", centerX, mainStartY + 2 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        mainMenuItems.emplace_back("Exit", centerX, mainStartY + 3 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        
        float totalSettingsHeight = numSettingsButtons * buttonHeight + (numSettingsButtons - 1) * buttonSpacing;
        float settingsStartY = winH / 2.0f - totalSettingsHeight / 2.0f;
        
        settingsMenuItems.clear();
        settingsMenuItems.emplace_back("Volume", centerX, settingsStartY + 0 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        settingsMenuItems.emplace_back("Toggle Fullscreen", centerX, settingsStartY + 1 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        settingsMenuItems.emplace_back("Back to Menu", centerX, settingsStartY + 2 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        
        // Initialize pause menu items
        float totalPauseHeight = numPauseButtons * buttonHeight + (numPauseButtons - 1) * buttonSpacing;
        float pauseStartY = winH / 2.0f - totalPauseHeight / 2.0f;
        
        pauseMenuItems.clear();
        pauseMenuItems.emplace_back("Resume", centerX, pauseStartY + 0 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        pauseMenuItems.emplace_back("Save Game", centerX, pauseStartY + 1 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
        pauseMenuItems.emplace_back("Main Menu", centerX, pauseStartY + 2 * (buttonHeight + buttonSpacing), buttonWidth, buttonHeight);
    }
    
    void Update() {
        // Workaround: force fullscreen resize for a few frames after toggling
        if (pendingFullscreenResize && fullscreenResizeFrames > 0) {
            int monitor = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            fullscreenResizeFrames--;
            if (fullscreenResizeFrames == 0) {
                pendingFullscreenResize = false;
            }
        }
        CheckInputMode();
        
        // Toggle controller debug overlay
        if (IsKeyPressed(KEY_F1)) {
            showControllerDebug = !showControllerDebug;
        }
        
        // Update save popup timer
        if (showSavePopup) {
            savePopupTimer -= GetFrameTime();
            if (savePopupTimer <= 0.0f) {
                showSavePopup = false;
            }
        }
        
        switch (currentState) {
            case GameState::MENU:
                UpdateMenu(mainMenuItems);
                break;
            case GameState::PLAYING:
                UpdateGame();
                break;
            case GameState::SETTINGS:
                UpdateMenu(settingsMenuItems);
                break;
            case GameState::PAUSED:
                UpdatePaused();
                break;
        }
    }
    
    void UpdateMenu(std::vector<MenuItem>& menuItems) {
        int currentWidth = GetScreenWidth();
        int currentHeight = GetScreenHeight();
        
        if (forceMenuRecalc || currentWidth != lastWindowWidth || currentHeight != lastWindowHeight) {
            InitializeMenus();
            forceMenuRecalc = false;
            lastWindowWidth = currentWidth;
            lastWindowHeight = currentHeight;
        }
        
        // Handle Escape key to go back from settings to main menu
        if (currentState == GameState::SETTINGS) {
            bool escapePressed = IsKeyPressed(KEY_ESCAPE);
            bool backButtonPressed = controller && IsButtonJustPressed(this->backButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK));
            bool bButtonPressed = controller && IsButtonJustPressed(this->bButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B));
            
            if (escapePressed || backButtonPressed || bButtonPressed) {
                SaveGame(); // Auto-save when going back to menu
                currentState = GameState::MENU;
                return;
            }
        }
        
        for (auto& item : menuItems) {
            item.isSelected = false;
            item.isHovered = false; // Clear hover states initially
        }
        
        if (currentInputMode == InputMode::KEYBOARD_MOUSE) {
            Vector2 mousePos = GetMousePosition();
            bool mouseUsed = false;
            bool mouseHovering = false;
            bool keyboardUsed = false;
            
            // Check for keyboard navigation first
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || 
                IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
                IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                keyboardControllerNavigationUsed = true;
                keyboardUsed = true;
            }
            
            // Only set hover states if not using keyboard navigation
            if (!keyboardControllerNavigationUsed) {
                for (auto& item : menuItems) {
                    item.isHovered = CheckCollisionPointRec(mousePos, item.bounds);
                    if (item.isHovered) {
                        mouseHovering = true;
                    }
                    // Volume menu item mouse +/-
                    if (currentState == GameState::SETTINGS && item.text == "Volume") {
                        float btnSize = item.bounds.height * 0.7f;
                        Rectangle minusBtn = {item.bounds.x + 8, item.bounds.y + item.bounds.height/2 - btnSize/2, btnSize, btnSize};
                        Rectangle plusBtn = {item.bounds.x + item.bounds.width - btnSize - 8, item.bounds.y + item.bounds.height/2 - btnSize/2, btnSize, btnSize};
                        if (CheckCollisionPointRec(mousePos, minusBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            volume = std::clamp(volume - 0.05f, 0.0f, 1.0f);
                            volume = roundf(volume * 20.0f) / 20.0f;
                            SetMasterVolume(volume);
                            if (soundLoaded) PlaySound(volumeChangeSound);
                            SaveGame();
                        }
                        if (CheckCollisionPointRec(mousePos, plusBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            volume = std::clamp(volume + 0.05f, 0.0f, 1.0f);
                            volume = roundf(volume * 20.0f) / 20.0f;
                            SetMasterVolume(volume);
                            if (soundLoaded) PlaySound(volumeChangeSound);
                            SaveGame();
                        }
                    }
                    if (item.isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        HandleMenuClick(item.text);
                        mouseUsed = true;
                    }
                }
            }
            
            // Volume adjustment with keyboard (A/D/Left/Right) when selected
            if (currentState == GameState::SETTINGS && menuItems[selectedMenuItem].text == "Volume") {
                bool left = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
                bool right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
                if (left) {
                    volume = std::clamp(volume - 0.05f, 0.0f, 1.0f);
                    volume = roundf(volume * 20.0f) / 20.0f;
                    SetMasterVolume(volume);
                    if (soundLoaded) PlaySound(volumeChangeSound);
                    SaveGame();
                }
                if (right) {
                    volume = std::clamp(volume + 0.05f, 0.0f, 1.0f);
                    volume = roundf(volume * 20.0f) / 20.0f;
                    SetMasterVolume(volume);
                    if (soundLoaded) PlaySound(volumeChangeSound);
                    SaveGame();
                }
            }
            
            // Show keyboard selection if keyboard was recently used, regardless of mouse position
            if (keyboardControllerNavigationUsed) {
                // Up/Down arrow or W/S
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                    selectedMenuItem = (selectedMenuItem + 1) % menuItems.size();
                }
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                    selectedMenuItem = (selectedMenuItem - 1 + menuItems.size()) % menuItems.size();
                }
                // Select current item
                if (selectedMenuItem >= 0 && selectedMenuItem < menuItems.size()) {
                    menuItems[selectedMenuItem].isSelected = true;
                }
                // Enter to select
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    if (selectedMenuItem >= 0 && selectedMenuItem < menuItems.size()) {
                        HandleMenuClick(menuItems[selectedMenuItem].text);
                    }
                }
            }
            
            // Reset keyboard navigation flag if mouse is used
            if (mouseUsed) {
                keyboardControllerNavigationUsed = false;
            }
        } else {
            // SDL2 controller navigation
            if (controller) {
                keyboardControllerNavigationUsed = true;
                
                // Clear all hover states when controller is used
                for (auto& item : menuItems) {
                    item.isHovered = false;
                }
                
                // Get current button states
                bool dPadUp = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
                bool dPadDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                bool dPadLeft = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                bool dPadRight = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                bool aButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
                bool bButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
                bool startButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
                bool backButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK);
                
                // Get analog stick values
                Sint16 leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                Sint16 leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
                float leftXFloat = leftX / 32767.0f;
                float leftYFloat = leftY / 32767.0f;
                
                // Check for D-pad navigation (one press at a time)
                if (IsButtonJustPressed(dPadUpPressed, dPadUp) || 
                    IsKeyPressed(KEY_UP) ||
                    IsAnalogDirectionJustPressed(analogUpPressed, -leftYFloat, analogStickThreshold)) {
                    selectedMenuItem = (selectedMenuItem - 1 + menuItems.size()) % menuItems.size();
                }
                
                if (IsButtonJustPressed(dPadDownPressed, dPadDown) || 
                    IsKeyPressed(KEY_DOWN) ||
                    IsAnalogDirectionJustPressed(analogDownPressed, leftYFloat, analogStickThreshold)) {
                    selectedMenuItem = (selectedMenuItem + 1) % menuItems.size();
                }
                
                if (selectedMenuItem >= 0 && selectedMenuItem < menuItems.size()) {
                    menuItems[selectedMenuItem].isSelected = true;
                }
                
                // Handle selection with A button
                if (IsButtonJustPressed(aButtonPressed, aButton) || 
                    IsKeyPressed(KEY_ENTER)) {
                    if (selectedMenuItem >= 0 && selectedMenuItem < menuItems.size()) {
                        HandleMenuClick(menuItems[selectedMenuItem].text);
                    }
                }
                
                // Volume adjustment with controller (one jump per press)
                static bool dPadLeftPressedVol = false;
                static bool dPadRightPressedVol = false;
                if (currentState == GameState::SETTINGS && menuItems[selectedMenuItem].text == "Volume") {
                    bool dpadLeft = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                    bool dpadRight = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                    if (IsButtonJustPressed(dPadLeftPressedVol, dpadLeft)) {
                        volume = std::clamp(volume - 0.05f, 0.0f, 1.0f);
                        volume = roundf(volume * 20.0f) / 20.0f;
                        SetMasterVolume(volume);
                        if (soundLoaded) PlaySound(volumeChangeSound);
                        SaveGame();
                    }
                    if (IsButtonJustPressed(dPadRightPressedVol, dpadRight)) {
                        volume = std::clamp(volume + 0.05f, 0.0f, 1.0f);
                        volume = roundf(volume * 20.0f) / 20.0f;
                        SetMasterVolume(volume);
                        if (soundLoaded) PlaySound(volumeChangeSound);
                        SaveGame();
                    }
                }
            }
        }
    }
    
    void HandleMenuClick(const std::string& itemText) {
        if (currentState == GameState::MENU) {
            if (itemText == "Start Game") {
                SetPlayerPositionFromSave();
                currentState = GameState::PLAYING;
            } else if (itemText == "Settings") {
                currentState = GameState::SETTINGS;
            } else if (itemText == "Save Game") {
                SaveGame();
            } else if (itemText == "Exit") {
                SaveGame(); // Auto-save on exit
                shouldExit = true;
            }
        } else if (currentState == GameState::SETTINGS) {
            if (itemText == "Volume") {
                // Implement volume change logic
            } else if (itemText == "Toggle Fullscreen") {
                if (isFullscreen) {
                    // Switch to windowed mode with title bar
                    SetWindowState(FLAG_WINDOW_RESIZABLE);
                    SetWindowSize(1280, 720); // Set a reasonable windowed size
                } else {
                    // Switch to fullscreen mode - force proper resolution
                    int monitor = GetCurrentMonitor();
                    int monitorWidth = GetMonitorWidth(monitor);
                    int monitorHeight = GetMonitorHeight(monitor);
                    SetWindowState(FLAG_FULLSCREEN_MODE);
                    // Workaround: force resize for a few frames
                    pendingFullscreenResize = true;
                    fullscreenResizeFrames = 10;
                }
                isFullscreen = !isFullscreen;
                // Add a longer delay for fullscreen toggle
                for (int i = 0; i < 3; i++) {
                    EndDrawing();
                    BeginDrawing();
                    ClearBackground({30, 30, 46, 255});
                    EndDrawing();
                }
                // Recalculate player position for new window size
                SetPlayerPositionFromSave();
                forceMenuRecalc = true; // Force recalculation after delay
            } else if (itemText == "Back to Menu") {
                SaveGame(); // Auto-save when going back to menu
                currentState = GameState::MENU;
            }
        } else if (currentState == GameState::PAUSED) {
            if (itemText == "Resume") {
                currentState = GameState::PLAYING;
            } else if (itemText == "Save Game") {
                SaveGame();
            } else if (itemText == "Main Menu") {
                SaveGame(); // Auto-save when going to main menu
                currentState = GameState::MENU;
            }
        }
    }
    
    void UpdateGame() {
        // Handle input
        if (IsKeyPressed(KEY_ESCAPE) || 
            (controller && IsButtonJustPressed(startButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START)))) {
            currentState = GameState::PAUSED;
        }
        
        // Player movement
        Vector2 movement = {0, 0};
        
        if (currentInputMode == InputMode::KEYBOARD_MOUSE) {
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) movement.y -= 1;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) movement.y += 1;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) movement.x -= 1;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) movement.x += 1;
        } else {
            // SDL2 controller movement
            if (controller) {
                Sint16 leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                Sint16 leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
                
                // Convert to float and apply deadzone
                float deadzone = 8000.0f; // SDL2 uses -32768 to 32767
                if (abs(leftX) > deadzone) {
                    movement.x = leftX / 32767.0f;
                }
                if (abs(leftY) > deadzone) {
                    movement.y = leftY / 32767.0f;
                }
            }
        }
        
        // Normalize diagonal movement
        if (movement.x != 0 && movement.y != 0) {
            movement.x *= 0.707f;
            movement.y *= 0.707f;
        }
        
        // Calculate relative movement speed based on window size
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        float baseSpeed = std::min(screenW, screenH) * 0.5f;
        
        playerPos.x += movement.x * baseSpeed * GetFrameTime();
        playerPos.y += movement.y * baseSpeed * GetFrameTime();
        
        // Keep player on screen
        float playerSize = GetScreenWidth() * 0.03f;
        playerPos.x = std::clamp(playerPos.x, 0.0f, (float)(GetScreenWidth() - playerSize));
        playerPos.y = std::clamp(playerPos.y, 0.0f, (float)(GetScreenHeight() - playerSize));
    }
    
    void UpdatePaused() {
        if (IsKeyPressed(KEY_ESCAPE) || 
            (controller && IsButtonJustPressed(startButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))) ||
            (controller && IsButtonJustPressed(bButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B)))) {
            currentState = GameState::PLAYING;
        }
        if (IsKeyPressed(KEY_M) || 
            (controller && IsButtonJustPressed(backButtonPressed, SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK)))) {
            currentState = GameState::MENU;
        }
        
        int currentWidth = GetScreenWidth();
        int currentHeight = GetScreenHeight();
        
        if (currentWidth != lastWindowWidth || currentHeight != lastWindowHeight) {
            InitializeMenus();
            lastWindowWidth = currentWidth;
            lastWindowHeight = currentHeight;
        }
        
        for (auto& item : pauseMenuItems) {
            item.isSelected = false;
            item.isHovered = false; // Clear hover states initially
        }
        
        if (currentInputMode == InputMode::KEYBOARD_MOUSE) {
            Vector2 mousePos = GetMousePosition();
            bool mouseUsed = false;
            bool mouseHovering = false;
            bool keyboardUsed = false;
            
            // Check for keyboard navigation first
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || 
                IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
                IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                keyboardControllerNavigationUsed = true;
                keyboardUsed = true;
            }
            
            // Only set hover states if not using keyboard navigation
            if (!keyboardControllerNavigationUsed) {
                for (auto& item : pauseMenuItems) {
                    item.isHovered = CheckCollisionPointRec(mousePos, item.bounds);
                    if (item.isHovered) {
                        mouseHovering = true;
                    }
                    
                    if (item.isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        HandleMenuClick(item.text);
                        mouseUsed = true;
                    }
                }
            }
            
            // Show keyboard selection if keyboard was recently used, regardless of mouse position
            if (keyboardControllerNavigationUsed) {
                // Up/Down arrow or W/S
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                    selectedMenuItem = (selectedMenuItem + 1) % pauseMenuItems.size();
                }
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                    selectedMenuItem = (selectedMenuItem - 1 + pauseMenuItems.size()) % pauseMenuItems.size();
                }
                // Select current item
                if (selectedMenuItem >= 0 && selectedMenuItem < pauseMenuItems.size()) {
                    pauseMenuItems[selectedMenuItem].isSelected = true;
                }
                // Enter to select
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    if (selectedMenuItem >= 0 && selectedMenuItem < pauseMenuItems.size()) {
                        HandleMenuClick(pauseMenuItems[selectedMenuItem].text);
                    }
                }
            }
            
            // Reset keyboard navigation flag if mouse is used
            if (mouseUsed) {
                keyboardControllerNavigationUsed = false;
            }
        } else {
            if (controller) {
                keyboardControllerNavigationUsed = true;
                
                // Clear all hover states when controller is used
                for (auto& item : pauseMenuItems) {
                    item.isHovered = false;
                }
                
                // Get current button states
                bool dPadUp = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
                bool dPadDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                bool aButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
                bool bButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
                
                // Get analog stick values
                Sint16 leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                Sint16 leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
                float leftXFloat = leftX / 32767.0f;
                float leftYFloat = leftY / 32767.0f;
                
                if (IsButtonJustPressed(dPadUpPressed, dPadUp) || 
                    IsKeyPressed(KEY_UP) ||
                    IsAnalogDirectionJustPressed(analogUpPressed, -leftYFloat, analogStickThreshold)) {
                    selectedMenuItem = (selectedMenuItem - 1 + pauseMenuItems.size()) % pauseMenuItems.size();
                }
                if (IsButtonJustPressed(dPadDownPressed, dPadDown) || 
                    IsKeyPressed(KEY_DOWN) ||
                    IsAnalogDirectionJustPressed(analogDownPressed, leftYFloat, analogStickThreshold)) {
                    selectedMenuItem = (selectedMenuItem + 1) % pauseMenuItems.size();
                }
                
                if (selectedMenuItem >= 0 && selectedMenuItem < pauseMenuItems.size()) {
                    pauseMenuItems[selectedMenuItem].isSelected = true;
                }
                
                if (IsButtonJustPressed(aButtonPressed, aButton) || 
                    IsKeyPressed(KEY_ENTER)) {
                    if (selectedMenuItem >= 0 && selectedMenuItem < pauseMenuItems.size()) {
                        HandleMenuClick(pauseMenuItems[selectedMenuItem].text);
                    }
                }
            }
        }
    }
    
    void Draw() {
        BeginDrawing();
        ClearBackground({30, 30, 46, 255});
        ClearBackground({30, 30, 46, 255}); // Catppuccin Mocha background (#1e1e2e)
        
        switch (currentState) {
            case GameState::MENU:
                DrawMenu(mainMenuItems, "2D Game Template");
                break;
            case GameState::PLAYING:
                DrawGame();
                break;
            case GameState::SETTINGS:
                DrawMenu(settingsMenuItems, "Settings");
                break;
            case GameState::PAUSED:
                DrawPaused();
                break;
        }
        
        // Draw save popup if needed
        if (showSavePopup) {
            int winW = GetScreenWidth();
            int winH = GetScreenHeight();
            int popupFontSize = winH * 0.025f;
            const char* popupText = "Game Saved!";
            int textWidth = MeasureText(popupText, popupFontSize);
            float alpha = (savePopupTimer / savePopupDuration);
            Color popupColor = Fade(GREEN, alpha);
            DrawText(popupText, winW - textWidth - 30, 30, popupFontSize, popupColor);
        }
        
        // Draw controller debug overlay if enabled
        if (showControllerDebug) {
            DrawControllerDebugOverlay();
        }
        
        EndDrawing();
    }

    void DrawMenu(const std::vector<MenuItem>& menuItems, const std::string& title) {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        
        // Scale title size relative to window
        int titleSize = winH * 0.05f; // 5% of window height
        int titleWidth = MeasureText(title.c_str(), titleSize);
        DrawText(title.c_str(), winW / 2 - titleWidth / 2, winH * 0.1f, titleSize, DARKGRAY);
        
        for (size_t i = 0; i < menuItems.size(); ++i) {
            const auto& item = menuItems[i];
            Color drawColor = item.color;
            if (item.isHovered || item.isSelected) {
                drawColor = item.hoverColor;
            }
            DrawRectangleRec(item.bounds, drawColor);
            DrawRectangleLinesEx(item.bounds, 2, BLACK);
            
            // Volume menu item special rendering
            if (currentState == GameState::SETTINGS && item.text == "Volume") {
                // Draw volume percentage
                int textSize = item.bounds.height * 0.5f;
                std::string volText = "Volume: " + std::to_string((int)(volume * 100)) + "%";
                int textWidth = MeasureText(volText.c_str(), textSize);
                DrawText(volText.c_str(), 
                    item.bounds.x + item.bounds.width / 2 - textWidth / 2,
                    item.bounds.y + item.bounds.height / 2 - textSize / 2,
                    textSize, WHITE);
                // Draw - and + buttons
                float btnSize = item.bounds.height * 0.7f;
                Rectangle minusBtn = {item.bounds.x + 8, item.bounds.y + item.bounds.height/2 - btnSize/2, btnSize, btnSize};
                Rectangle plusBtn = {item.bounds.x + item.bounds.width - btnSize - 8, item.bounds.y + item.bounds.height/2 - btnSize/2, btnSize, btnSize};
                DrawRectangleRec(minusBtn, GRAY);
                DrawRectangleRec(plusBtn, GRAY);
                DrawRectangleLinesEx(minusBtn, 2, BLACK);
                DrawRectangleLinesEx(plusBtn, 2, BLACK);
                // Draw - and + symbols
                int symbolSize = btnSize * 0.6f;
                int minusX = minusBtn.x + btnSize/2 - symbolSize/2;
                int minusY = minusBtn.y + btnSize/2 - symbolSize/8;
                DrawRectangle(minusX, minusY, symbolSize, symbolSize/4, BLACK);
                int plusX = plusBtn.x + btnSize/2 - symbolSize/2;
                int plusY = plusBtn.y + btnSize/2 - symbolSize/8;
                DrawRectangle(plusX, plusY, symbolSize, symbolSize/4, BLACK);
                DrawRectangle(plusX + symbolSize/2 - symbolSize/8, plusY - symbolSize/2 + symbolSize/8, symbolSize/4, symbolSize, BLACK);
            } else {
                // Scale text size relative to button size
                int textSize = item.bounds.height * 0.5f;
                int textWidth = MeasureText(item.text.c_str(), textSize);
                DrawText(item.text.c_str(), 
                        item.bounds.x + item.bounds.width / 2 - textWidth / 2,
                        item.bounds.y + item.bounds.height / 2 - textSize / 2,
                        textSize, WHITE);
            }
        }
        
        // Scale instruction text
        int instructionSize = winH * 0.02f; // 2% of window height
        std::string instructionText = currentInputMode == InputMode::KEYBOARD_MOUSE ? 
            "Use mouse to navigate" : "Use controller D-pad to navigate, A to select";
        DrawText(instructionText.c_str(), 10, winH - instructionSize - 10, instructionSize, GRAY);
        
        // Show current input mode
        std::string inputModeText = currentInputMode == InputMode::KEYBOARD_MOUSE ? 
            "Input: Keyboard/Mouse" : "Input: Controller";
        DrawText(inputModeText.c_str(), winW - MeasureText(inputModeText.c_str(), instructionSize) - 10, 
                winH - instructionSize - 10, instructionSize, GRAY);
    }
    
    void DrawGame() {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        
        // Scale player size relative to window
        float playerSize = winW * 0.03f; // 3% of window width
        DrawRectangle(playerPos.x, playerPos.y, playerSize, playerSize, BLUE);
        DrawRectangleLines(playerPos.x, playerPos.y, playerSize, playerSize, DARKBLUE);
        
        // Scale UI text sizes relative to window
        int titleSize = winH * 0.04f; // 4% of window height
        int subtitleSize = winH * 0.025f; // 2.5% of window height
        int infoSize = winH * 0.02f; // 2% of window height
        
        // Scale UI positioning relative to window
        int margin = winW * 0.01f; // 1% of window width
        int lineSpacing = winH * 0.03f; // 3% of window height
        
        DrawText("Game Running", margin, margin, titleSize, DARKGRAY);
        
        std::string controlsText = currentInputMode == InputMode::KEYBOARD_MOUSE ? 
            "WASD/Arrow Keys: Move" : "Left Stick: Move";
        DrawText(controlsText.c_str(), margin, margin + lineSpacing, subtitleSize, GRAY);
        
        std::string pauseText = currentInputMode == InputMode::KEYBOARD_MOUSE ? 
            "ESC: Pause" : "Start Button: Pause";
        DrawText(pauseText.c_str(), margin, margin + lineSpacing * 2, subtitleSize, GRAY);
        
        // Draw player position with scaled text
        std::string posText = "Player: (" + std::to_string((int)playerPos.x) + 
                             ", " + std::to_string((int)playerPos.y) + ")";
        DrawText(posText.c_str(), margin, margin + lineSpacing * 3, infoSize, GRAY);
        
        // Show current input mode
        std::string inputModeText = currentInputMode == InputMode::KEYBOARD_MOUSE ? 
            "Input: Keyboard/Mouse" : "Input: Controller";
        DrawText(inputModeText.c_str(), winW - MeasureText(inputModeText.c_str(), infoSize) - margin, 
                margin, infoSize, GRAY);
    }
    
    void DrawPaused() {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        
        // Draw semi-transparent overlay
        DrawRectangle(0, 0, winW, winH, {0, 0, 0, 128});
        
        // Draw pause menu
        DrawMenu(pauseMenuItems, "PAUSED");
    }
    
    void DrawControllerDebugOverlay() {
        int y = 40;
        int fontSize = 18;
        DrawRectangle(20, 20, 700, 400, Fade(BLACK, 0.7f));
        DrawText("[Controller Debug - F1 to hide]", 30, y, fontSize, YELLOW);
        y += fontSize + 8;
        
        if (controller) {
            DrawText("Controller: Connected", 30, y, fontSize, GREEN);
            y += fontSize + 2;
            
            // Show axis values
            Sint16 leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
            DrawText(TextFormat("Left Stick: (%d, %d)", leftX, leftY), 50, y, fontSize, LIGHTGRAY);
            y += fontSize;
            
            // Show button states
            bool aButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
            bool bButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
            bool startButton = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
            DrawText(TextFormat("Buttons - A: %s, B: %s, Start: %s", 
                               aButton ? "YES" : "NO", bButton ? "YES" : "NO", startButton ? "YES" : "NO"), 
                    50, y, fontSize, LIGHTGRAY);
        } else {
            DrawText("Controller: Not connected", 30, y, fontSize, RED);
        }
    }
};

int main() {
    std::cout << "=== RUNNING LATEST BUILD ===" << std::endl;
    Game game;
    game.Run();
    CloseWindow();
    return 0;
}