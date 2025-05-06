/**
 * app.js
 * Main JavaScript file for the Clair Obscur: Expedition 33 Tracker
 */
document.addEventListener('DOMContentLoaded', () => {
    // --- Application State ---
    let appData = {
        quests: [],
        characters: [],
        inventory: [],
        locations: [],
        notes: [],
        settings: {
            theme: 'light', // Future use
            lastExport: null
        }
    };

    const APP_DATA_KEY = 'coe33TrackerData';

    // --- DOM Elements Cache ---
    // General UI
    const sections = document.querySelectorAll('.app-section');
    const navLinks = document.querySelectorAll('.navbar-nav .nav-link');

    // Dashboard Elements
    const dashboardActiveQuests = document.getElementById('dashboardActiveQuests');
    const dashboardTotalQuests = document.getElementById('dashboardTotalQuests');
    const dashboardTotalCharacters = document.getElementById('dashboardTotalCharacters');
    const dashboardTotalInventory = document.getElementById('dashboardTotalInventory');
    const dashboardTotalLocations = document.getElementById('dashboardTotalLocations');
    const dashboardDiscoveredLocations = document.getElementById('dashboardDiscoveredLocations');
    const dashboardTotalNotes = document.getElementById('dashboardTotalNotes');
    const lastExportTime = document.getElementById('lastExportTime');
    const dashboardGoToButtons = document.querySelectorAll('#dashboardSection .btn[data-section-target]');

    // Quest Elements
    const questListElement = document.getElementById('questList');
    const questForm = document.getElementById('questForm');
    const questModalElement = document.getElementById('questModal');
    const questModal = new bootstrap.Modal(questModalElement);
    const questModalLabel = document.getElementById('questModalLabel');
    const saveQuestBtn = document.getElementById('saveQuestBtn');
    const addQuestBtn = document.getElementById('addQuestBtn');
    const noQuestsMessage = document.getElementById('noQuestsMessage');
    const questSearchInput = document.getElementById('questSearch');
    const questFilterStatus = document.getElementById('questFilterStatus');
    const questFilterType = document.getElementById('questFilterType');

    // Character Elements
    const characterListElement = document.getElementById('characterList');
    const characterForm = document.getElementById('characterForm');
    const characterModalElement = document.getElementById('characterModal');
    const characterModal = new bootstrap.Modal(characterModalElement);
    const characterModalLabel = document.getElementById('characterModalLabel');
    const saveCharacterBtn = document.getElementById('saveCharacterBtn');
    const addCharacterBtn = document.getElementById('addCharacterBtn');
    const noCharactersMessage = document.getElementById('noCharactersMessage');
    const characterSearchInput = document.getElementById('characterSearch');

    // Inventory Elements
    const inventoryListElement = document.getElementById('inventoryList');
    const inventoryForm = document.getElementById('inventoryForm');
    const inventoryModalElement = document.getElementById('inventoryModal');
    const inventoryModal = new bootstrap.Modal(inventoryModalElement);
    const inventoryModalLabel = document.getElementById('inventoryModalLabel');
    const saveInventoryBtn = document.getElementById('saveInventoryBtn');
    const addInventoryBtn = document.getElementById('addInventoryBtn');
    const noInventoryMessage = document.getElementById('noInventoryMessage');
    const inventorySearchInput = document.getElementById('inventorySearch');

    // Location Elements
    const locationListElement = document.getElementById('locationList');
    const locationForm = document.getElementById('locationForm');
    const locationModalElement = document.getElementById('locationModal');
    const locationModal = new bootstrap.Modal(locationModalElement);
    const locationModalLabel = document.getElementById('locationModalLabel');
    const saveLocationBtn = document.getElementById('saveLocationBtn');
    const addLocationBtn = document.getElementById('addLocationBtn');
    const noLocationsMessage = document.getElementById('noLocationsMessage');
    const locationSearchInput = document.getElementById('locationSearch');

    // Note Elements
    const noteListElement = document.getElementById('noteList');
    const noteForm = document.getElementById('noteForm');
    const noteModalElement = document.getElementById('noteModal');
    const noteModal = new bootstrap.Modal(noteModalElement);
    const noteModalLabel = document.getElementById('noteModalLabel');
    const saveNoteBtn = document.getElementById('saveNoteBtn');
    const addNoteBtn = document.getElementById('addNoteBtn');
    const noNotesMessage = document.getElementById('noNotesMessage');
    const noteSearchInput = document.getElementById('noteSearch');

    // Settings / Data Management Elements
    const settingsModalElement = document.getElementById('settingsModal');
    // const settingsModal = new bootstrap.Modal(settingsModalElement); // Already initialized if needed elsewhere
    const exportDataBtn = document.getElementById('exportDataBtn');
    const importDataBtn = document.getElementById('importDataBtn');
    const exportDataBtnSettings = document.getElementById('exportDataBtnSettings');
    const importDataBtnSettings = document.getElementById('importDataBtnSettings');
    const importFileInput = document.getElementById('importFile');
    const clearAllDataBtn = document.getElementById('clearAllDataBtn');


    // --- Core Functions ---

    /**
     * Loads application data from localStorage or sets default structure.
     */
    function loadData() {
        const savedData = localStorage.getItem(APP_DATA_KEY);
        const defaultData = {
            quests: [], characters: [], inventory: [], locations: [], notes: [],
            settings: { theme: 'light', lastExport: null }
        };
        if (savedData) {
            try {
                // Merge saved data with defaults to ensure all keys exist
                const parsedData = JSON.parse(savedData);
                appData = { ...defaultData, ...parsedData };
                // Ensure nested settings object also merges correctly
                appData.settings = { ...defaultData.settings, ...(parsedData.settings || {}) };
                console.log('Data loaded from localStorage.');
            } catch (e) {
                console.error("Failed to parse saved data:", e);
                alert("Error loading saved data. It might be corrupted. Starting fresh.");
                appData = defaultData; // Fallback to default
            }
        } else {
            appData = defaultData;
            console.log('No saved data found, starting fresh.');
        }
        // applyTheme(appData.settings.theme || 'light'); // Apply theme if/when implemented
    }

    /**
     * Saves the current application data state to localStorage.
     */
    function saveData() {
        try {
            localStorage.setItem(APP_DATA_KEY, JSON.stringify(appData));
            console.log('Data saved to localStorage.');
            updateDashboardStats(); // Update dashboard whenever data changes
        } catch (e) {
             console.error("Failed to save data to localStorage:", e);
             alert("Error saving data! LocalStorage might be full or unavailable.");
        }
    }

    /**
     * Generates a unique ID using the UUID library.
     * Requires the UUID library to be loaded correctly.
     * @returns {string} A unique v4 UUID.
     */
    function generateId() {
        // Ensure UUID is loaded - handle potential script load errors gracefully
        if (typeof uuid === 'undefined') {
            console.error('UUID library not loaded!');
            // Fallback to a less reliable method if UUID fails
             return Date.now().toString(36) + Math.random().toString(36).substring(2);
        }
        return uuid.v4();
    }

    /**
     * Switches the visible section in the main content area.
     * @param {string} sectionId - The ID of the section to show (e.g., 'quests', 'characters').
     */
    function switchSection(sectionId) {
        sections.forEach(section => section.classList.add('d-none'));
        navLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('data-section') === sectionId) {
                link.classList.add('active');
            }
        });

        const activeSection = document.getElementById(`${sectionId}Section`);
        if (activeSection) {
            activeSection.classList.remove('d-none');
            // Automatically focus the search input if it exists in the section
            const searchInput = activeSection.querySelector('input[type="text"][id$="Search"]');
            if(searchInput) {
                 setTimeout(() => searchInput.focus(), 100); // Delay slightly for transition
            }
        } else {
            console.warn(`Section ${sectionId} not found! Falling back to dashboard.`);
            // Ensure dashboard is shown if target section fails
            const dashboardSection = document.getElementById('dashboardSection');
            if(dashboardSection) dashboardSection.classList.remove('d-none');
             // Reset active nav link to dashboard
             navLinks.forEach(link => link.classList.toggle('active', link.getAttribute('data-section') === 'dashboard'));
        }
        window.scrollTo(0, 0); // Scroll to top on section change
    }

     /**
      * Escapes HTML special characters in a string for safe rendering.
      * @param {string} str - The input string.
      * @returns {string} The escaped string.
      */
    function escapeHTML(str) {
         if (!str) return '';
         return str.replace(/[&<>"']/g, function (match) {
             return {
                 '&': '&',
                 '<': '<',
                 '>': '>',
                 '"': '"',
                 "'": '\'' // Use numeric entity for single quote
             }[match];
         });
     }

    // --- Quest Module ---

    /**
     * Renders the quest list based on current filters and search term.
     */
    function renderQuests() {
        if (!questListElement) return; // Exit if element doesn't exist
        questListElement.innerHTML = '';
        const searchTerm = questSearchInput.value.toLowerCase().trim();
        const statusFilter = questFilterStatus.value;
        const typeFilter = questFilterType.value;

        let filteredQuests = appData.quests;

        // Apply status filter
        if (statusFilter === 'active') {
            filteredQuests = filteredQuests.filter(q => !q.isCompleted);
        } else if (statusFilter === 'completed') {
            filteredQuests = filteredQuests.filter(q => q.isCompleted);
        }

        // Apply type filter
         if (typeFilter === 'main') {
            filteredQuests = filteredQuests.filter(q => q.isMain);
        } else if (typeFilter === 'side') {
            filteredQuests = filteredQuests.filter(q => !q.isMain);
        }

        // Apply search
        if (searchTerm) {
            filteredQuests = filteredQuests.filter(q =>
                q.name.toLowerCase().includes(searchTerm) ||
                (q.description && q.description.toLowerCase().includes(searchTerm)) ||
                (q.location && q.location.toLowerCase().includes(searchTerm)) ||
                (q.giver && q.giver.toLowerCase().includes(searchTerm))
            );
        }

        noQuestsMessage.style.display = filteredQuests.length === 0 ? 'block' : 'none';

        filteredQuests.sort((a, b) => (a.name || '').localeCompare(b.name || ''));
        filteredQuests.forEach(quest => {
            const item = document.createElement('div');
            item.className = `list-group-item list-group-item-action quest-item ${quest.isCompleted ? 'completed' : ''}`;
            item.setAttribute('data-id', quest.id);

            item.innerHTML = `
                <div class="d-flex w-100 justify-content-between">
                    <h5 class="mb-1 quest-name">${escapeHTML(quest.name)} ${quest.isMain ? '<span class="badge bg-warning text-dark ms-2">Main</span>' : ''}</h5>
                    <small class="item-actions">
                         <button class="btn btn-sm btn-outline-primary edit-quest-btn" title="Edit"><i class="fas fa-edit"></i></button>
                         <button class="btn btn-sm btn-outline-success toggle-complete-quest-btn" title="${quest.isCompleted ? 'Mark Active' : 'Mark Complete'}"><i class="fas ${quest.isCompleted ? 'fa-undo' : 'fa-check'}"></i></button>
                         <button class="btn btn-sm btn-outline-danger delete-quest-btn" title="Delete"><i class="fas fa-trash"></i></button>
                    </small>
                </div>
                ${quest.description ? `<p class="mb-1">${escapeHTML(quest.description).replace(/\n/g, '<br>')}</p>` : ''}
                <small class="text-muted">
                    ${quest.location ? `<i class="fas fa-map-marker-alt me-1"></i> ${escapeHTML(quest.location)}` : ''}
                    ${quest.giver ? `<span class="ms-2"><i class="fas fa-user me-1"></i> ${escapeHTML(quest.giver)}</span>` : ''}
                </small>
            `;
            item.querySelector('.edit-quest-btn').addEventListener('click', (e) => { e.stopPropagation(); editQuest(quest.id); });
            item.querySelector('.toggle-complete-quest-btn').addEventListener('click', (e) => { e.stopPropagation(); toggleQuestComplete(quest.id); });
            item.querySelector('.delete-quest-btn').addEventListener('click', (e) => { e.stopPropagation(); deleteQuest(quest.id); });
            questListElement.appendChild(item);
        });
    }

    /**
     * Saves a quest (new or edited) from the modal form.
     */
    function saveQuest() {
        const questId = document.getElementById('questId').value;
        const quest = {
            id: questId || generateId(),
            name: document.getElementById('questName').value.trim(),
            description: document.getElementById('questDescription').value.trim(),
            location: document.getElementById('questLocation').value.trim(),
            giver: document.getElementById('questGiver').value.trim(),
            isMain: document.getElementById('questIsMain').checked,
            isCompleted: document.getElementById('questIsCompleted').checked,
        };
        if (!quest.name) { alert("Quest Name is required."); return; }
        const index = appData.quests.findIndex(q => q.id === quest.id);
        if (index > -1) appData.quests[index] = quest; else appData.quests.push(quest);
        saveData();
        renderQuests();
        questModal.hide();
    }

    /** Clears the quest modal form. */
    function clearQuestForm() {
        questForm.reset();
        document.getElementById('questId').value = '';
        questModalLabel.textContent = 'Add New Quest';
        // Ensure checkboxes are explicitly false after reset
        document.getElementById('questIsMain').checked = false;
        document.getElementById('questIsCompleted').checked = false;
    }

    /**
     * Populates the quest modal for editing and shows it.
     * @param {string} id - The ID of the quest to edit.
     */
    function editQuest(id) { // *** CORRECTED FUNCTION ***
        const quest = appData.quests.find(q => q.id === id);
        if (quest) {
            clearQuestForm();
            questModalLabel.textContent = 'Edit Quest';
            // Populate form fields based on quest object properties
            document.getElementById('questId').value = quest.id;
            document.getElementById('questName').value = quest.name || '';
            document.getElementById('questDescription').value = quest.description || '';
            document.getElementById('questLocation').value = quest.location || '';
            document.getElementById('questGiver').value = quest.giver || '';
            document.getElementById('questIsMain').checked = quest.isMain || false;
            document.getElementById('questIsCompleted').checked = quest.isCompleted || false;
            questModal.show();
        } else {
            console.error("Quest not found for editing:", id);
            alert("Could not find the quest to edit.");
        }
    }

    /**
     * Deletes a quest after confirmation.
     * @param {string} id - The ID of the quest to delete.
     */
    function deleteQuest(id) {
        const quest = appData.quests.find(q => q.id === id);
        if (quest && confirm(`Are you sure you want to delete the quest "${quest.name || 'this quest'}"?`)) {
            appData.quests = appData.quests.filter(q => q.id !== id);
            saveData();
            renderQuests();
        }
    }

     /**
      * Toggles the completion status of a quest.
      * @param {string} id - The ID of the quest to toggle.
      */
    function toggleQuestComplete(id) {
        const index = appData.quests.findIndex(q => q.id === id);
        if (index > -1) {
            appData.quests[index].isCompleted = !appData.quests[index].isCompleted;
            saveData();
            renderQuests();
        }
    }

    // --- Character Module ---

    /**
     * Renders the character list based on search term.
     */
    function renderCharacters() {
        if (!characterListElement) return;
        characterListElement.innerHTML = '';
        const searchTerm = characterSearchInput.value.toLowerCase().trim();
        let filteredChars = appData.characters;

        if (searchTerm) {
            filteredChars = filteredChars.filter(c =>
                c.name.toLowerCase().includes(searchTerm) ||
                (c.notes && c.notes.toLowerCase().includes(searchTerm))
            );
        }

        noCharactersMessage.style.display = filteredChars.length === 0 ? 'block' : 'none';

        filteredChars.sort((a, b) => (a.name || '').localeCompare(b.name || ''));
        filteredChars.forEach(char => {
            const col = document.createElement('div');
            col.className = 'col';
            const card = document.createElement('div');
            card.className = 'card h-100 character-card';
            card.setAttribute('data-id', char.id);
            card.innerHTML = `
                <div class="card-body d-flex flex-column">
                    <div class="card-title">
                        <h5 class="mb-0">${escapeHTML(char.name)} ${char.isInParty ? '<i class="fas fa-users text-primary ms-2" title="In Party"></i>' : ''}</h5>
                         <div class="item-actions">
                            <button class="btn btn-sm btn-outline-primary edit-character-btn" title="Edit"><i class="fas fa-edit"></i></button>
                            <button class="btn btn-sm btn-outline-danger delete-character-btn" title="Delete"><i class="fas fa-trash"></i></button>
                        </div>
                    </div>
                    <p class="card-text small text-muted flex-grow-1">${escapeHTML(char.notes || '').substring(0, 100)}${char.notes && char.notes.length > 100 ? '...' : ''}</p>
                     <!-- Add level/stats display later -->
                </div>
            `;
             card.querySelector('.edit-character-btn').addEventListener('click', (e) => { e.stopPropagation(); editCharacter(char.id); });
             card.querySelector('.delete-character-btn').addEventListener('click', (e) => { e.stopPropagation(); deleteCharacter(char.id); });
            col.appendChild(card);
            characterListElement.appendChild(col);
        });
    }

    /**
     * Saves a character (new or edited) from the modal form.
     */
    function saveCharacter() {
        const charId = document.getElementById('characterId').value;
        const character = {
            id: charId || generateId(),
            name: document.getElementById('characterName').value.trim(),
            notes: document.getElementById('characterNotes').value.trim(),
            isInParty: document.getElementById('characterIsInParty').checked,
        };
        if (!character.name) { alert("Character Name is required."); return; }
        const index = appData.characters.findIndex(c => c.id === character.id);
        if (index > -1) appData.characters[index] = character; else appData.characters.push(character);
        saveData();
        renderCharacters();
        characterModal.hide();
    }

    /** Clears the character modal form. */
    function clearCharacterForm() {
        characterForm.reset();
        document.getElementById('characterId').value = '';
        characterModalLabel.textContent = 'Add New Character';
         document.getElementById('characterIsInParty').checked = false;
    }

    /**
     * Populates the character modal for editing and shows it.
     * @param {string} id - The ID of the character to edit.
     */
    function editCharacter(id) { // *** CORRECTED FUNCTION ***
        const character = appData.characters.find(c => c.id === id);
        if (character) {
            clearCharacterForm();
            characterModalLabel.textContent = 'Edit Character';
            document.getElementById('characterId').value = character.id;
            document.getElementById('characterName').value = character.name || '';
            document.getElementById('characterNotes').value = character.notes || '';
            document.getElementById('characterIsInParty').checked = character.isInParty || false;
            characterModal.show();
        } else {
             console.error("Character not found for editing:", id);
             alert("Could not find the character to edit.");
        }
    }

    /**
     * Deletes a character after confirmation.
     * @param {string} id - The ID of the character to delete.
     */
    function deleteCharacter(id) {
        const character = appData.characters.find(c => c.id === id);
        if (character && confirm(`Are you sure you want to delete the character "${character.name || 'this character'}"?`)) {
            appData.characters = appData.characters.filter(c => c.id !== id);
            saveData();
            renderCharacters();
        }
    }

    // --- Inventory Module ---

    /**
     * Renders the inventory list based on search term.
     */
    function renderInventory() {
        if (!inventoryListElement) return;
        inventoryListElement.innerHTML = '';
        const searchTerm = inventorySearchInput.value.toLowerCase().trim();
        let filteredItems = appData.inventory;

        if (searchTerm) {
             filteredItems = filteredItems.filter(i =>
                 i.name.toLowerCase().includes(searchTerm) ||
                 (i.description && i.description.toLowerCase().includes(searchTerm)) ||
                 (i.type && i.type.toLowerCase().includes(searchTerm))
             );
        }

         noInventoryMessage.style.display = filteredItems.length === 0 ? 'block' : 'none';

         filteredItems.sort((a, b) => (a.type || '').localeCompare(b.type || '') || (a.name || '').localeCompare(b.name || '')); // Sort by type, then name
         filteredItems.forEach(item => {
             const div = document.createElement('div');
             div.className = 'list-group-item list-group-item-action inventory-item';
             div.setAttribute('data-id', item.id);
             div.innerHTML = `
                 <div class="d-flex w-100 justify-content-between">
                     <h6 class="mb-1">
                        ${escapeHTML(item.name)}
                        ${item.quantity > 1 ? `<span class="badge bg-secondary ms-2">x${item.quantity}</span>` : ''}
                        ${item.isKeyItem ? '<span class="badge bg-info text-dark ms-2">Key</span>' : ''}
                        ${item.type ? `<span class="badge bg-light text-dark border ms-2">${escapeHTML(item.type)}</span>` : ''}
                     </h6>
                     <small class="item-actions">
                         <button class="btn btn-sm btn-outline-primary edit-inventory-btn" title="Edit"><i class="fas fa-edit"></i></button>
                         <button class="btn btn-sm btn-outline-danger delete-inventory-btn" title="Delete"><i class="fas fa-trash"></i></button>
                     </small>
                 </div>
                 ${item.description ? `<p class="mb-1 small text-muted">${escapeHTML(item.description)}</p>` : ''}
             `;
             div.querySelector('.edit-inventory-btn').addEventListener('click', (e) => { e.stopPropagation(); editInventory(item.id); });
             div.querySelector('.delete-inventory-btn').addEventListener('click', (e) => { e.stopPropagation(); deleteInventory(item.id); });
             inventoryListElement.appendChild(div);
         });
    }

    /**
     * Saves an inventory item (new or edited) from the modal form.
     */
    function saveInventory() {
        const itemId = document.getElementById('inventoryId').value;
        const item = {
            id: itemId || generateId(),
            name: document.getElementById('inventoryName').value.trim(),
            quantity: parseInt(document.getElementById('inventoryQuantity').value, 10) || 0, // Default to 0 if parsing fails
            type: document.getElementById('inventoryType').value.trim(),
            description: document.getElementById('inventoryDescription').value.trim(),
            isKeyItem: document.getElementById('inventoryIsKeyItem').checked,
        };
        if (!item.name) { alert("Item Name is required."); return; }
        if (isNaN(item.quantity) || item.quantity < 0) item.quantity = 0; // Ensure quantity is a non-negative number

        const index = appData.inventory.findIndex(i => i.id === item.id);
        if (index > -1) appData.inventory[index] = item; else appData.inventory.push(item);
        saveData();
        renderInventory();
        inventoryModal.hide();
    }

    /** Clears the inventory modal form. */
     function clearInventoryForm() {
         inventoryForm.reset();
         document.getElementById('inventoryId').value = '';
         document.getElementById('inventoryQuantity').value = 1; // Default quantity
         inventoryModalLabel.textContent = 'Add New Item';
         document.getElementById('inventoryIsKeyItem').checked = false;
     }

     /**
      * Populates the inventory modal for editing and shows it.
      * @param {string} id - The ID of the item to edit.
      */
    function editInventory(id) { // *** CORRECTED FUNCTION ***
        const item = appData.inventory.find(i => i.id === id);
        if (item) {
            clearInventoryForm();
            inventoryModalLabel.textContent = 'Edit Item';
            document.getElementById('inventoryId').value = item.id;
            document.getElementById('inventoryName').value = item.name || '';
            // Use item.quantity directly, handle potential undefined in save if needed
            document.getElementById('inventoryQuantity').value = item.quantity;
            document.getElementById('inventoryType').value = item.type || '';
            document.getElementById('inventoryDescription').value = item.description || '';
            document.getElementById('inventoryIsKeyItem').checked = item.isKeyItem || false;
            inventoryModal.show();
        } else {
             console.error("Inventory item not found for editing:", id);
             alert("Could not find the item to edit.");
        }
    }

     /**
      * Deletes an inventory item after confirmation.
      * @param {string} id - The ID of the item to delete.
      */
    function deleteInventory(id) {
        const item = appData.inventory.find(i => i.id === id);
        if (item && confirm(`Are you sure you want to delete the item "${item.name || 'this item'}"?`)) {
            appData.inventory = appData.inventory.filter(i => i.id !== id);
            saveData();
            renderInventory();
        }
    }


    // --- Atlas/Location Module ---

    /**
     * Renders the location list based on search term.
     */
    function renderLocations() {
        if (!locationListElement) return;
        locationListElement.innerHTML = '';
        const searchTerm = locationSearchInput.value.toLowerCase().trim();
        let filteredLocs = appData.locations;

         if (searchTerm) {
             filteredLocs = filteredLocs.filter(l =>
                 l.name.toLowerCase().includes(searchTerm) ||
                 (l.region && l.region.toLowerCase().includes(searchTerm)) ||
                 (l.notes && l.notes.toLowerCase().includes(searchTerm))
             );
         }

         noLocationsMessage.style.display = filteredLocs.length === 0 ? 'block' : 'none';

         filteredLocs.sort((a, b) => (a.region || '').localeCompare(b.region || '') || (a.name || '').localeCompare(b.name || '')); // Sort by region, then name
         filteredLocs.forEach(loc => {
             const div = document.createElement('div');
             // Add discovered class for styling
             div.className = `list-group-item list-group-item-action location-item ${loc.isDiscovered ? 'discovered' : ''}`;
             div.setAttribute('data-id', loc.id);
             div.innerHTML = `
                 <div class="d-flex w-100 justify-content-between">
                     <h6 class="mb-1 location-name">${escapeHTML(loc.name)} ${loc.region ? `<small class="text-muted"> - ${escapeHTML(loc.region)}</small>` : ''}</h6>
                     <small class="item-actions">
                         <button class="btn btn-sm btn-outline-secondary toggle-discover-location-btn me-1" title="${loc.isDiscovered ? 'Mark Undiscovered' : 'Mark Discovered'}"><i class="fas ${loc.isDiscovered ? 'fa-eye-slash' : 'fa-eye'}"></i></button>
                         <button class="btn btn-sm btn-outline-primary edit-location-btn" title="Edit"><i class="fas fa-edit"></i></button>
                         <button class="btn btn-sm btn-outline-danger delete-location-btn" title="Delete"><i class="fas fa-trash"></i></button>
                     </small>
                 </div>
                 ${loc.notes ? `<p class="mb-1 small text-muted">${escapeHTML(loc.notes).replace(/\n/g, '<br>')}</p>` : ''}
             `;
             div.querySelector('.edit-location-btn').addEventListener('click', (e) => { e.stopPropagation(); editLocation(loc.id); });
             div.querySelector('.delete-location-btn').addEventListener('click', (e) => { e.stopPropagation(); deleteLocation(loc.id); });
             div.querySelector('.toggle-discover-location-btn').addEventListener('click', (e) => { e.stopPropagation(); toggleLocationDiscovered(loc.id); });
             locationListElement.appendChild(div);
         });
    }

    /**
     * Saves a location (new or edited) from the modal form.
     */
     function saveLocation() {
        const locId = document.getElementById('locationId').value;
        const location = {
            id: locId || generateId(),
            name: document.getElementById('locationName').value.trim(),
            region: document.getElementById('locationRegion').value.trim(),
            notes: document.getElementById('locationNotes').value.trim(),
            isDiscovered: document.getElementById('locationIsDiscovered').checked,
        };
        if (!location.name) { alert("Location Name is required."); return; }
        const index = appData.locations.findIndex(l => l.id === location.id);
        if (index > -1) appData.locations[index] = location; else appData.locations.push(location);
        saveData();
        renderLocations();
        locationModal.hide();
    }

    /** Clears the location modal form. */
     function clearLocationForm() {
         locationForm.reset();
         document.getElementById('locationId').value = '';
         locationModalLabel.textContent = 'Add New Location';
         document.getElementById('locationIsDiscovered').checked = false;
     }

    /**
     * Populates the location modal for editing and shows it.
     * @param {string} id - The ID of the location to edit.
     */
    function editLocation(id) { // *** CORRECTED FUNCTION ***
        const location = appData.locations.find(l => l.id === id);
        if (location) {
            clearLocationForm();
            locationModalLabel.textContent = 'Edit Location';
            document.getElementById('locationId').value = location.id;
            document.getElementById('locationName').value = location.name || '';
            document.getElementById('locationRegion').value = location.region || '';
            document.getElementById('locationNotes').value = location.notes || '';
            document.getElementById('locationIsDiscovered').checked = location.isDiscovered || false;
            locationModal.show();
        } else {
             console.error("Location not found for editing:", id);
             alert("Could not find the location to edit.");
        }
    }

    /**
     * Deletes a location after confirmation.
     * @param {string} id - The ID of the location to delete.
     */
     function deleteLocation(id) {
        const location = appData.locations.find(l => l.id === id);
        if (location && confirm(`Are you sure you want to delete the location "${location.name || 'this location'}"?`)) {
            appData.locations = appData.locations.filter(l => l.id !== id);
            saveData();
            renderLocations();
        }
    }

     /**
      * Toggles the discovered status of a location.
      * @param {string} id - The ID of the location to toggle.
      */
     function toggleLocationDiscovered(id) {
        const index = appData.locations.findIndex(l => l.id === id);
        if (index > -1) {
            appData.locations[index].isDiscovered = !appData.locations[index].isDiscovered;
            saveData();
            renderLocations(); // Re-render to update visual state
        }
    }


    // --- Note Module ---

    /**
     * Renders the notes list based on search term.
     */
    function renderNotes() {
        if (!noteListElement) return;
        noteListElement.innerHTML = '';
        const searchTerm = noteSearchInput.value.toLowerCase().trim();
        let filteredNotes = appData.notes;

         if (searchTerm) {
             filteredNotes = filteredNotes.filter(n =>
                 n.title.toLowerCase().includes(searchTerm) ||
                 (n.content && n.content.toLowerCase().includes(searchTerm))
             );
         }

         noNotesMessage.style.display = filteredNotes.length === 0 ? 'block' : 'none';

         filteredNotes.sort((a, b) => (a.title || '').localeCompare(b.title || '')); // Sort by title
         filteredNotes.forEach(note => {
             const col = document.createElement('div');
             col.className = 'col';
             const card = document.createElement('div');
             card.className = 'card h-100 note-card';
             card.setAttribute('data-id', note.id);
             card.style.cursor = 'pointer'; // Indicate clickable for editing
             card.innerHTML = `
                 <div class="card-body d-flex flex-column">
                      <div class="card-title">
                        <h6 class="mb-0">${escapeHTML(note.title)}</h6>
                         <div class="item-actions">
                             <button class="btn btn-sm btn-outline-primary edit-note-btn" title="Edit"><i class="fas fa-edit"></i></button>
                             <button class="btn btn-sm btn-outline-danger delete-note-btn" title="Delete"><i class="fas fa-trash"></i></button>
                         </div>
                    </div>
                    <p class="card-text small text-muted mt-2 flex-grow-1">${escapeHTML(note.content || '').substring(0, 150)}${note.content && note.content.length > 150 ? '...' : ''}</p>
                 </div>
             `;
             // Separate button listeners
             card.querySelector('.edit-note-btn').addEventListener('click', (e) => {
                e.stopPropagation(); // Prevent card click listener firing too
                editNote(note.id);
             });
             card.querySelector('.delete-note-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                deleteNote(note.id);
             });
             // Make card clickable to edit (excluding buttons)
             card.addEventListener('click', (e) => {
                 // Only trigger edit if the click wasn't on a button inside item-actions
                 if (!e.target.closest('.item-actions')) {
                     editNote(note.id);
                 }
             });

             col.appendChild(card);
             noteListElement.appendChild(col);
         });
    }

    /**
     * Saves a note (new or edited) from the modal form.
     */
     function saveNote() {
        const noteId = document.getElementById('noteId').value;
        const note = {
            id: noteId || generateId(),
            title: document.getElementById('noteTitle').value.trim(),
            content: document.getElementById('noteContent').value.trim(),
        };
        if (!note.title) { alert("Note Title is required."); return; }
        const index = appData.notes.findIndex(n => n.id === note.id);
        if (index > -1) appData.notes[index] = note; else appData.notes.push(note);
        saveData();
        renderNotes();
        noteModal.hide();
    }

    /** Clears the note modal form. */
     function clearNoteForm() {
         noteForm.reset();
         document.getElementById('noteId').value = '';
         noteModalLabel.textContent = 'Add New Note';
     }

     /**
      * Populates the note modal for editing and shows it.
      * @param {string} id - The ID of the note to edit.
      */
    function editNote(id) { // *** CORRECTED FUNCTION ***
        const note = appData.notes.find(n => n.id === id);
        if (note) {
            clearNoteForm();
            noteModalLabel.textContent = 'Edit Note';
            document.getElementById('noteId').value = note.id;
            document.getElementById('noteTitle').value = note.title || '';
            document.getElementById('noteContent').value = note.content || '';
            noteModal.show();
        } else {
            console.error("Note not found for editing:", id);
            alert("Could not find the note to edit.");
        }
    }

     /**
      * Deletes a note after confirmation.
      * @param {string} id - The ID of the note to delete.
      */
     function deleteNote(id) {
        const note = appData.notes.find(n => n.id === id);
        if (note && confirm(`Are you sure you want to delete the note "${note.title || 'this note'}"?`)) {
            appData.notes = appData.notes.filter(n => n.id !== id);
            saveData();
            renderNotes();
        }
    }


    // --- Dashboard Update ---

    /**
     * Updates the statistics displayed on the dashboard.
     */
    function updateDashboardStats() {
        // Check if elements exist before updating
        if(dashboardActiveQuests) dashboardActiveQuests.textContent = appData.quests.filter(q => !q.isCompleted).length;
        if(dashboardTotalQuests) dashboardTotalQuests.textContent = appData.quests.length;
        if(dashboardTotalCharacters) dashboardTotalCharacters.textContent = appData.characters.length;
        if(dashboardTotalInventory) dashboardTotalInventory.textContent = appData.inventory.length;

        const discoveredLocs = appData.locations.filter(l => l.isDiscovered).length;
        if(dashboardTotalLocations) dashboardTotalLocations.textContent = appData.locations.length;
        if(dashboardDiscoveredLocations) dashboardDiscoveredLocations.textContent = discoveredLocs;
        if(dashboardTotalNotes) dashboardTotalNotes.textContent = appData.notes.length;

        if (lastExportTime) {
            if (appData.settings.lastExport) {
                 try {
                    lastExportTime.textContent = new Date(appData.settings.lastExport).toLocaleString();
                } catch (e) {
                    console.error("Error formatting last export date:", e);
                    lastExportTime.textContent = "Invalid Date";
                }
            } else {
                lastExportTime.textContent = "Never";
            }
        }
    }

    // --- Data Management Functions ---

    /**
     * Exports the current application data as a JSON file download.
     */
    function exportData() {
        try {
             appData.settings.lastExport = new Date().toISOString();
             saveData(); // Save updated export time immediately

             const dataStr = JSON.stringify(appData, null, 2); // Pretty print JSON
             const dataBlob = new Blob([dataStr], { type: 'application/json;charset=utf-8' }); // Specify charset
             const url = URL.createObjectURL(dataBlob);
             const link = document.createElement('a');
             // Generate filename with timestamp
             const timestamp = new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-');
             link.download = `coe33-tracker-backup-${timestamp}.json`;
             link.href = url;
             link.style.display = 'none'; // Hide the link
             document.body.appendChild(link); // Append to body to ensure click works in all browsers
             link.click();
             document.body.removeChild(link); // Clean up link element
             URL.revokeObjectURL(url); // Clean up object URL
             console.log("Data exported successfully.");
             alert("Data exported successfully!");
             updateDashboardStats(); // Refresh dashboard to show new export time
         } catch(e) {
             console.error("Error exporting data:", e);
             alert(`An error occurred while exporting data: ${e.message}`);
         }
    }

    /**
     * Handles the file input change event for importing data.
     * @param {Event} event - The file input change event.
     */
    function handleImportFile(event) {
        const file = event.target.files[0];
        if (!file) {
            console.log("No file selected for import.");
            return;
        }
        if (file.type && file.type !== 'application/json') {
             alert(`Invalid file type: ${file.type}. Please select a JSON file.`);
             importFileInput.value = ''; // Reset file input
             return;
        }


        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const importedJson = e.target.result;
                const importedData = JSON.parse(importedJson);

                // Basic validation: Check if it looks like our app data structure
                if (importedData && typeof importedData === 'object' && (importedData.quests !== undefined || importedData.characters !== undefined || importedData.inventory !== undefined || importedData.locations !== undefined || importedData.notes !== undefined || importedData.settings !== undefined)) {
                     if (confirm("Importing will OVERWRITE all current data in this browser. Are you absolutely sure you want to proceed?")) {
                         // Re-construct appData ensuring all keys are present and merging settings
                         const defaultData = { quests: [], characters: [], inventory: [], locations: [], notes: [], settings: { theme: 'light', lastExport: null } };
                         appData = { ...defaultData, ...importedData };
                         appData.settings = { ...defaultData.settings, ...(importedData.settings || {}) };

                         saveData(); // Save the newly imported data

                         // Re-render everything based on imported data
                         renderQuests();
                         renderCharacters();
                         renderInventory();
                         renderLocations();
                         renderNotes();
                         updateDashboardStats(); // Ensure dashboard reflects new data
                         switchSection('dashboard'); // Go to dashboard after import
                         alert("Data imported successfully! All previous local data has been replaced.");
                         console.log("Data imported successfully.");
                    } else {
                        console.log("Import cancelled by user.");
                    }
                } else {
                     alert("Import failed: The selected file does not appear to contain valid tracker data structure.");
                     console.error("Import failed: Invalid file structure.");
                }
            } catch (error) {
                alert(`Import failed: Could not parse the JSON file. Is it valid?\nError: ${error.message}`);
                console.error("Import parsing failed:", error);
            } finally {
                 // Reset file input value so the same file can be selected again if needed
                 importFileInput.value = '';
            }
        };
        reader.onerror = (err) => {
             alert(`Error reading the selected file: ${err.message}`);
             console.error("File reading error:", err);
             importFileInput.value = '';
        }
        reader.readAsText(file); // Read file as text
    }

    /**
     * Clears all application data from localStorage after double confirmation.
     */
    function clearAllData() {
         if (confirm("DANGER! This will permanently delete ALL data stored in this browser for the COE33 tracker. This action cannot be undone!\n\nAre you absolutely sure? Consider exporting your data first!")) {
             if (confirm("FINAL CONFIRMATION: Really delete everything?")) {
                 localStorage.removeItem(APP_DATA_KEY);
                 // Reset appData to initial empty state by reloading defaults
                 loadData();
                 // Re-render all sections to reflect the empty state
                 renderQuests();
                 renderCharacters();
                 renderInventory();
                 renderLocations();
                 renderNotes();
                 updateDashboardStats(); // Resets dashboard counts
                 switchSection('dashboard'); // Switch back to dashboard
                 alert("All local data has been cleared.");
                 console.log("All local data cleared by user.");
             } else {
                 console.log("Clear data cancelled (second confirmation).");
             }
         } else {
             console.log("Clear data cancelled (first confirmation).");
         }
    }

    // --- Event Listeners Setup ---

    /**
     * Attaches all necessary event listeners on application start.
     */
    function initializeEventListeners() {
        // Navbar links
        navLinks.forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const sectionId = link.getAttribute('data-section');
                if (sectionId) {
                    switchSection(sectionId);
                }
            });
        });

        // Dashboard "Go To" buttons
         dashboardGoToButtons.forEach(button => {
             button.addEventListener('click', (e) => {
                 const targetSection = e.currentTarget.getAttribute('data-section-target');
                 if (targetSection) {
                     switchSection(targetSection);
                 }
             });
         });

        // --- Module Specific Listeners ---

        // Quest Listeners
        if(addQuestBtn) addQuestBtn.addEventListener('click', clearQuestForm);
        if(saveQuestBtn) saveQuestBtn.addEventListener('click', saveQuest);
        if(questModalElement) questModalElement.addEventListener('hidden.bs.modal', clearQuestForm); // Clear form when modal closes
        if(questSearchInput) questSearchInput.addEventListener('input', renderQuests);
        if(questFilterStatus) questFilterStatus.addEventListener('change', renderQuests);
        if(questFilterType) questFilterType.addEventListener('change', renderQuests);

        // Character Listeners
        if(addCharacterBtn) addCharacterBtn.addEventListener('click', clearCharacterForm);
        if(saveCharacterBtn) saveCharacterBtn.addEventListener('click', saveCharacter);
        if(characterModalElement) characterModalElement.addEventListener('hidden.bs.modal', clearCharacterForm);
        if(characterSearchInput) characterSearchInput.addEventListener('input', renderCharacters);

        // Inventory Listeners
        if(addInventoryBtn) addInventoryBtn.addEventListener('click', clearInventoryForm);
        if(saveInventoryBtn) saveInventoryBtn.addEventListener('click', saveInventory);
        if(inventoryModalElement) inventoryModalElement.addEventListener('hidden.bs.modal', clearInventoryForm);
        if(inventorySearchInput) inventorySearchInput.addEventListener('input', renderInventory);

        // Location Listeners
        if(addLocationBtn) addLocationBtn.addEventListener('click', clearLocationForm);
        if(saveLocationBtn) saveLocationBtn.addEventListener('click', saveLocation);
        if(locationModalElement) locationModalElement.addEventListener('hidden.bs.modal', clearLocationForm);
        if(locationSearchInput) locationSearchInput.addEventListener('input', renderLocations);

        // Note Listeners
        if(addNoteBtn) addNoteBtn.addEventListener('click', clearNoteForm);
        if(saveNoteBtn) saveNoteBtn.addEventListener('click', saveNote);
        if(noteModalElement) noteModalElement.addEventListener('hidden.bs.modal', clearNoteForm);
        if(noteSearchInput) noteSearchInput.addEventListener('input', renderNotes);

        // --- Data Management Listeners ---
         const triggerImport = () => importFileInput.click();

         if(exportDataBtn) exportDataBtn.addEventListener('click', exportData);
         if(importDataBtn) importDataBtn.addEventListener('click', triggerImport);
         if(exportDataBtnSettings) exportDataBtnSettings.addEventListener('click', exportData);
         if(importDataBtnSettings) importDataBtnSettings.addEventListener('click', triggerImport);
         if(importFileInput) importFileInput.addEventListener('change', handleImportFile);
         if(clearAllDataBtn) clearAllDataBtn.addEventListener('click', clearAllData);

        console.log("Event listeners initialized.");
    }


    // --- Application Initialization ---
    loadData();
    initializeEventListeners();
    switchSection('dashboard'); // Start on the dashboard

    // Initial render of all sections to display any loaded data
    renderQuests();
    renderCharacters();
    renderInventory();
    renderLocations();
    renderNotes();
    updateDashboardStats(); // Update dashboard on load with initial counts/info

    console.log("COE33 Tracker Initialized and Ready.");
});