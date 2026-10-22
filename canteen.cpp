#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <ctime>
#include <algorithm>
#include <limits>
#include <map>

using namespace std;

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================
const string MENU_FILE   = "canteen_menu.txt";
const string USERS_FILE  = "canteen_users.txt";
const string ORDERS_FILE = "canteen_orders.txt";
const string ADMIN_PIN   = "9999";
const double TAX_RATE    = 0.05; // 5% GST/Service Tax

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pauseConsole() {
    cout << "\nPress Enter to continue...";
    cin.get();
}

string toUpperCase(string str) {
    for (char &c : str) {
        c = (char)toupper((unsigned char)c);
    }
    return str;
}

string getCurrentTimestamp() {
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);
    char buffer[64];
    if (ltm) {
        strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M", ltm);
        return string(buffer);
    }
    return "N/A";
}

int getValidInt(const string &prompt, int minVal = 0, int maxVal = 1000000) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val) {
            if (val >= minVal && val <= maxVal) {
                clearInputBuffer();
                return val;
            }
            cout << ">> Please enter a value between " << minVal << " and " << maxVal << ".\n";
        } else {
            cout << ">> Invalid input! Please enter a valid number.\n";
            clearInputBuffer();
        }
    }
}

double getValidDouble(const string &prompt, double minVal = 0.0, double maxVal = 100000.0) {
    double val;
    while (true) {
        cout << prompt;
        if (cin >> val) {
            if (val >= minVal && val <= maxVal) {
                clearInputBuffer();
                return val;
            }
            cout << ">> Please enter a value between " << minVal << " and " << maxVal << ".\n";
        } else {
            cout << ">> Invalid input! Please enter a valid number.\n";
            clearInputBuffer();
        }
    }
}

string getValidLine(const string &prompt) {
    string str;
    while (true) {
        cout << prompt;
        getline(cin, str);
        // Trim leading and trailing spaces
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first != string::npos) {
            size_t last = str.find_last_not_of(" \t\r\n");
            return str.substr(first, (last - first + 1));
        }
        cout << ">> Input cannot be empty! Please try again.\n";
    }
}

// ============================================================================
// DATA MODELS
// ============================================================================
struct MenuItem {
    int id;
    string name;
    string category; // Breakfast, Meals, Snacks, Beverages, Desserts
    double price;
    int stock;
    int prepTimeMin;
    bool isVeg;

    string serialize() const {
        stringstream ss;
        ss << id << "|" << name << "|" << category << "|" << price << "|" 
           << stock << "|" << prepTimeMin << "|" << (isVeg ? 1 : 0);
        return ss.str();
    }

    static bool deserialize(const string &line, MenuItem &item) {
        stringstream ss(line);
        string idStr, name, cat, priceStr, stockStr, prepStr, vegStr;
        if (getline(ss, idStr, '|') &&
            getline(ss, name, '|') &&
            getline(ss, cat, '|') &&
            getline(ss, priceStr, '|') &&
            getline(ss, stockStr, '|') &&
            getline(ss, prepStr, '|') &&
            getline(ss, vegStr, '|')) {
            try {
                item.id = stoi(idStr);
                item.name = name;
                item.category = cat;
                item.price = stod(priceStr);
                item.stock = stoi(stockStr);
                item.prepTimeMin = stoi(prepStr);
                item.isVeg = (stoi(vegStr) == 1);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};

struct CartItem {
    int itemId;
    string name;
    string category;
    double price;
    int quantity;
    string notes;
    int prepTimeMin;

    double getSubtotal() const {
        return price * quantity;
    }
};

struct UserAccount {
    string studentId;
    string name;
    string pin;
    double walletBalance;
    int loyaltyPoints;

    string serialize() const {
        stringstream ss;
        ss << studentId << "|" << name << "|" << pin << "|" << walletBalance << "|" << loyaltyPoints;
        return ss.str();
    }

    static bool deserialize(const string &line, UserAccount &acc) {
        stringstream ss(line);
        string id, name, pin, balStr, ptsStr;
        if (getline(ss, id, '|') &&
            getline(ss, name, '|') &&
            getline(ss, pin, '|') &&
            getline(ss, balStr, '|') &&
            getline(ss, ptsStr, '|')) {
            try {
                acc.studentId = id;
                acc.name = name;
                acc.pin = pin;
                acc.walletBalance = stod(balStr);
                acc.loyaltyPoints = stoi(ptsStr);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};

enum OrderStatus {
    STATUS_PLACED = 0,
    STATUS_PREPARING,
    STATUS_READY,
    STATUS_COMPLETED,
    STATUS_CANCELLED
};

string statusToString(OrderStatus status) {
    switch (status) {
        case STATUS_PLACED:    return "PLACED";
        case STATUS_PREPARING: return "PREPARING";
        case STATUS_READY:     return "READY FOR PICKUP";
        case STATUS_COMPLETED: return "COMPLETED";
        case STATUS_CANCELLED: return "CANCELLED";
        default:               return "UNKNOWN";
    }
}

OrderStatus intToStatus(int val) {
    if (val >= 0 && val <= 4) return (OrderStatus)val;
    return STATUS_PLACED;
}

struct Order {
    int orderId;
    int tokenNumber;
    string studentId;
    string customerName;
    string timestamp;
    vector<CartItem> items;
    double subtotal;
    double discount;
    double tax;
    double totalAmount;
    string paymentMethod;
    OrderStatus status;
    int estimatedTimeMin;

    string serialize() const {
        stringstream ss;
        ss << orderId << "|" << tokenNumber << "|" << studentId << "|" << customerName << "|"
           << timestamp << "|" << subtotal << "|" << discount << "|" << tax << "|"
           << totalAmount << "|" << paymentMethod << "|" << (int)status << "|"
           << estimatedTimeMin << "|";

        // Serialize items: id#name#cat#price#qty#notes#prep;
        for (size_t i = 0; i < items.size(); ++i) {
            ss << items[i].itemId << "#" << items[i].name << "#" << items[i].category << "#"
               << items[i].price << "#" << items[i].quantity << "#" << items[i].notes << "#"
               << items[i].prepTimeMin;
            if (i + 1 < items.size()) ss << ";";
        }
        return ss.str();
    }

    static bool deserialize(const string &line, Order &order) {
        stringstream ss(line);
        string oIdStr, tokStr, sId, cName, timeStr, subStr, discStr, taxStr, totStr, payStr, statStr, estStr, itemsStr;
        if (getline(ss, oIdStr, '|') &&
            getline(ss, tokStr, '|') &&
            getline(ss, sId, '|') &&
            getline(ss, cName, '|') &&
            getline(ss, timeStr, '|') &&
            getline(ss, subStr, '|') &&
            getline(ss, discStr, '|') &&
            getline(ss, taxStr, '|') &&
            getline(ss, totStr, '|') &&
            getline(ss, payStr, '|') &&
            getline(ss, statStr, '|') &&
            getline(ss, estStr, '|') &&
            getline(ss, itemsStr, '|')) {
            try {
                order.orderId = stoi(oIdStr);
                order.tokenNumber = stoi(tokStr);
                order.studentId = sId;
                order.customerName = cName;
                order.timestamp = timeStr;
                order.subtotal = stod(subStr);
                order.discount = stod(discStr);
                order.tax = stod(taxStr);
                order.totalAmount = stod(totStr);
                order.paymentMethod = payStr;
                order.status = intToStatus(stoi(statStr));
                order.estimatedTimeMin = stoi(estStr);
                order.items.clear();

                if (!itemsStr.empty()) {
                    stringstream itemStream(itemsStr);
                    string singleItem;
                    while (getline(itemStream, singleItem, ';')) {
                        stringstream is(singleItem);
                        string iId, iName, iCat, iPrice, iQty, iNotes, iPrep;
                        if (getline(is, iId, '#') &&
                            getline(is, iName, '#') &&
                            getline(is, iCat, '#') &&
                            getline(is, iPrice, '#') &&
                            getline(is, iQty, '#') &&
                            getline(is, iNotes, '#') &&
                            getline(is, iPrep, '#')) {
                            CartItem ci;
                            ci.itemId = stoi(iId);
                            ci.name = iName;
                            ci.category = iCat;
                            ci.price = stod(iPrice);
                            ci.quantity = stoi(iQty);
                            ci.notes = iNotes;
                            ci.prepTimeMin = stoi(iPrep);
                            order.items.push_back(ci);
                        }
                    }
                }
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};

// ============================================================================
// MENU MANAGER CLASS
// ============================================================================
class MenuManager {
private:
    vector<MenuItem> menu;

    void loadDefaultMenu() {
        menu = {
            {101, "Masala Dosa",             "Breakfast", 50.00,  30, 6,  true},
            {102, "Idli Vada Combo",         "Breakfast", 40.00,  25, 4,  true},
            {103, "Paneer Cheese Sandwich",  "Snacks",    65.00,  20, 7,  true},
            {104, "Crispy Veg Burger",        "Snacks",    75.00,  20, 8,  true},
            {105, "Grilled Chicken Burger",  "Snacks",    95.00,  15, 10, false},
            {106, "Veg Fried Rice",          "Meals",     90.00,  18, 9,  true},
            {107, "Chicken Biryani",         "Meals",     140.00, 15, 12, false},
            {108, "Special Veg Thali",       "Meals",     110.00, 12, 10, true},
            {109, "Cold Coffee with Cream",  "Beverages", 45.00,  35, 3,  true},
            {110, "Fresh Lime Soda",         "Beverages", 35.00,  40, 3,  true},
            {111, "Chocolate Brownie",       "Desserts",  60.00,  15, 2,  true},
            {112, "Hot Gulab Jamun (2 pcs)", "Desserts",  35.00,  20, 2,  true}
        };
        saveToFile();
    }

public:
    MenuManager() {
        loadFromFile();
    }

    void loadFromFile() {
        menu.clear();
        ifstream fin(MENU_FILE);
        if (!fin.is_open()) {
            loadDefaultMenu();
            return;
        }

        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            MenuItem item;
            if (MenuItem::deserialize(line, item)) {
                menu.push_back(item);
            }
        }
        fin.close();

        if (menu.empty()) {
            loadDefaultMenu();
        }
    }

    void saveToFile() const {
        ofstream fout(MENU_FILE);
        if (!fout.is_open()) return;
        for (const auto &item : menu) {
            fout << item.serialize() << "\n";
        }
        fout.close();
    }

    const vector<MenuItem>& getItems() const {
        return menu;
    }

    MenuItem* findItemById(int id) {
        for (auto &item : menu) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    bool updateStock(int id, int qtyDelta) {
        MenuItem *item = findItemById(id);
        if (item) {
            if (item->stock + qtyDelta >= 0) {
                item->stock += qtyDelta;
                saveToFile();
                return true;
            }
        }
        return false;
    }

    void displayMenuTable(const vector<MenuItem> &itemsToDisplay) const {
        cout << "\n+-----+-------------------------------+-------------+------------+-------+--------+---------+\n";
        cout << "| ID  | Item Name                     | Category    | Price (Rs) | Stock | Prep(m)| Type    |\n";
        cout << "+-----+-------------------------------+-------------+------------+-------+--------+---------+\n";

        if (itemsToDisplay.empty()) {
            cout << "| No items match your selection.                                                       |\n";
        } else {
            for (const auto &item : itemsToDisplay) {
                string vegTag = item.isVeg ? "[Veg]" : "[Non-Veg]";
                string stockTag = (item.stock == 0) ? "OUT" : to_string(item.stock);

                cout << "| " << setw(3) << left << item.id << " | "
                     << setw(29) << left << item.name << " | "
                     << setw(11) << left << item.category << " | "
                     << setw(10) << right << fixed << setprecision(2) << item.price << " | "
                     << setw(5)  << right << stockTag << " | "
                     << setw(6)  << right << item.prepTimeMin << " | "
                     << setw(7)  << left << vegTag << " |\n";
            }
        }
        cout << "+-----+-------------------------------+-------------+------------+-------+--------+---------+\n";
    }

    void displayAll() const {
        displayMenuTable(menu);
    }

    void displayByCategory(const string &category) const {
        vector<MenuItem> filtered;
        for (const auto &item : menu) {
            if (toUpperCase(item.category) == toUpperCase(category)) {
                filtered.push_back(item);
            }
        }
        displayMenuTable(filtered);
    }

    void displayVegOnly() const {
        vector<MenuItem> filtered;
        for (const auto &item : menu) {
            if (item.isVeg) filtered.push_back(item);
        }
        displayMenuTable(filtered);
    }

    void searchByName(const string &keyword) const {
        vector<MenuItem> filtered;
        string kwUpper = toUpperCase(keyword);
        for (const auto &item : menu) {
            if (toUpperCase(item.name).find(kwUpper) != string::npos ||
                toUpperCase(item.category).find(kwUpper) != string::npos) {
                filtered.push_back(item);
            }
        }
        displayMenuTable(filtered);
    }

    bool addItem(const MenuItem &item) {
        if (findItemById(item.id) != nullptr) return false;
        menu.push_back(item);
        saveToFile();
        return true;
    }

    bool editPrice(int id, double newPrice) {
        MenuItem *item = findItemById(id);
        if (item) {
            item->price = newPrice;
            saveToFile();
            return true;
        }
        return false;
    }

    bool restockItem(int id, int addedStock) {
        MenuItem *item = findItemById(id);
        if (item && addedStock > 0) {
            item->stock += addedStock;
            saveToFile();
            return true;
        }
        return false;
    }

    bool removeItem(int id) {
        for (auto it = menu.begin(); it != menu.end(); ++it) {
            if (it->id == id) {
                menu.erase(it);
                saveToFile();
                return true;
            }
        }
        return false;
    }

    int generateNextId() const {
        int maxId = 100;
        for (const auto &item : menu) {
            if (item.id > maxId) maxId = item.id;
        }
        return maxId + 1;
    }

    void displayLowStockAlerts(int threshold = 5) const {
        vector<MenuItem> lowStock;
        for (const auto &item : menu) {
            if (item.stock <= threshold) {
                lowStock.push_back(item);
            }
        }

        cout << "\n=======================================================\n";
        cout << "            LOW INVENTORY ALERTS (Stock <= " << threshold << ")\n";
        cout << "=======================================================\n";
        if (lowStock.empty()) {
            cout << ">> All inventory levels are healthy! No low-stock items.\n";
        } else {
            for (const auto &item : lowStock) {
                cout << " [!] Item #" << item.id << " - " << item.name 
                     << " | Available: " << item.stock << " units left!\n";
            }
        }
    }
};

// ============================================================================
// USER & WALLET MANAGER CLASS
// ============================================================================
class UserManager {
private:
    vector<UserAccount> users;

    void loadDefaultUsers() {
        users = {
            {"S101", "Rahul Sharma", "1234", 500.00, 25},
            {"S102", "Priya Patel",  "1111", 350.00, 15},
            {"S103", "Amit Kumar",   "2222", 750.00, 40},
            {"S104", "Sneha Reddy",  "3333", 200.00, 10}
        };
        saveToFile();
    }

public:
    UserManager() {
        loadFromFile();
    }

    void loadFromFile() {
        users.clear();
        ifstream fin(USERS_FILE);
        if (!fin.is_open()) {
            loadDefaultUsers();
            return;
        }

        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            UserAccount acc;
            if (UserAccount::deserialize(line, acc)) {
                users.push_back(acc);
            }
        }
        fin.close();

        if (users.empty()) {
            loadDefaultUsers();
        }
    }

    void saveToFile() const {
        ofstream fout(USERS_FILE);
        if (!fout.is_open()) return;
        for (const auto &u : users) {
            fout << u.serialize() << "\n";
        }
        fout.close();
    }

    UserAccount* findUser(const string &studentId) {
        string searchId = toUpperCase(studentId);
        for (auto &u : users) {
            if (toUpperCase(u.studentId) == searchId) return &u;
        }
        return nullptr;
    }

    UserAccount* authenticate(const string &studentId, const string &pin) {
        UserAccount *u = findUser(studentId);
        if (u && u->pin == pin) {
            return u;
        }
        return nullptr;
    }

    bool registerUser(const string &studentId, const string &name, const string &pin, double initialBalance = 0.0) {
        if (findUser(studentId) != nullptr) return false;
        UserAccount newAcc = {toUpperCase(studentId), name, pin, initialBalance, 0};
        users.push_back(newAcc);
        saveToFile();
        return true;
    }

    bool rechargeWallet(const string &studentId, double amount) {
        UserAccount *u = findUser(studentId);
        if (u && amount > 0) {
            u->walletBalance += amount;
            saveToFile();
            return true;
        }
        return false;
    }

    bool deductWallet(const string &studentId, double amount) {
        UserAccount *u = findUser(studentId);
        if (u && amount > 0 && u->walletBalance >= amount) {
            u->walletBalance -= amount;
            // Earn loyalty points: 1 point per Rs. 50 spent
            int pointsEarned = (int)(amount / 50.0);
            u->loyaltyPoints += pointsEarned;
            saveToFile();
            return true;
        }
        return false;
    }

    void redeemLoyaltyPoints(const string &studentId, int points) {
        UserAccount *u = findUser(studentId);
        if (u && points > 0 && u->loyaltyPoints >= points) {
            u->loyaltyPoints -= points;
            saveToFile();
        }
    }

    void displayAllUsers() const {
        cout << "\n+------------+----------------------+------------+----------------+\n";
        cout << "| Student ID | Student Name         | Balance(Rs)| Loyalty Points |\n";
        cout << "+------------+----------------------+------------+----------------+\n";
        for (const auto &u : users) {
            cout << "| " << setw(10) << left << u.studentId << " | "
                 << setw(20) << left << u.name << " | "
                 << setw(10) << right << fixed << setprecision(2) << u.walletBalance << " | "
                 << setw(14) << right << u.loyaltyPoints << " |\n";
        }
        cout << "+------------+----------------------+------------+----------------+\n";
    }
};

// ============================================================================
// ORDER MANAGER CLASS
// ============================================================================
class OrderManager {
private:
    vector<Order> orders;

    void loadDefaultOrders() {
        orders.clear();
        saveToFile();
    }

public:
    OrderManager() {
        loadFromFile();
    }

    void loadFromFile() {
        orders.clear();
        ifstream fin(ORDERS_FILE);
        if (!fin.is_open()) return;

        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            Order ord;
            if (Order::deserialize(line, ord)) {
                orders.push_back(ord);
            }
        }
        fin.close();
    }

    void saveToFile() const {
        ofstream fout(ORDERS_FILE);
        if (!fout.is_open()) return;
        for (const auto &ord : orders) {
            fout << ord.serialize() << "\n";
        }
        fout.close();
    }

    int generateNextOrderId() const {
        int maxId = 1000;
        for (const auto &o : orders) {
            if (o.orderId > maxId) maxId = o.orderId;
        }
        return maxId + 1;
    }

    int generateNextToken() const {
        int maxToken = 100;
        for (const auto &o : orders) {
            if (o.tokenNumber > maxToken) maxToken = o.tokenNumber;
        }
        return maxToken + 1;
    }

    int getActiveKitchenQueueCount() const {
        int count = 0;
        for (const auto &o : orders) {
            if (o.status == STATUS_PLACED || o.status == STATUS_PREPARING) {
                count++;
            }
        }
        return count;
    }

    int calculateEstimatedWaitTime(const vector<CartItem> &items) const {
        int maxItemPrep = 0;
        for (const auto &item : items) {
            if (item.prepTimeMin > maxItemPrep) {
                maxItemPrep = item.prepTimeMin;
            }
        }
        // Smart wait time = item's prep time + (queue ahead * 2 mins)
        int queueAhead = getActiveKitchenQueueCount();
        return maxItemPrep + (queueAhead * 2);
    }

    void addOrder(const Order &order) {
        orders.push_back(order);
        saveToFile();
    }

    Order* findOrderByToken(int tokenNumber) {
        for (auto &o : orders) {
            if (o.tokenNumber == tokenNumber) return &o;
        }
        return nullptr;
    }

    Order* findOrderById(int orderId) {
        for (auto &o : orders) {
            if (o.orderId == orderId) return &o;
        }
        return nullptr;
    }

    vector<Order> getOrdersByStudent(const string &studentId) const {
        vector<Order> res;
        string sUpper = toUpperCase(studentId);
        for (const auto &o : orders) {
            if (toUpperCase(o.studentId) == sUpper) {
                res.push_back(o);
            }
        }
        return res;
    }

    const vector<Order>& getAllOrders() const {
        return orders;
    }

    void displayKitchenQueue() const {
        cout << "\n========================================================================================\n";
        cout << "                           LIVE KITCHEN DISPLAY SYSTEM (KDS)                            \n";
        cout << "========================================================================================\n";
        cout << "+-------+----------+----------------------+--------------------+--------------------+-------+\n";
        cout << "| Token | Order ID | Customer Name        | Status             | Payment            | Wait  |\n";
        cout << "+-------+----------+----------------------+--------------------+--------------------+-------+\n";

        bool hasActive = false;
        for (const auto &o : orders) {
            if (o.status == STATUS_PLACED || o.status == STATUS_PREPARING || o.status == STATUS_READY) {
                hasActive = true;
                string waitStr = to_string(o.estimatedTimeMin) + " min";
                cout << "| #" << setw(4) << left << o.tokenNumber << " | "
                     << setw(8)  << left << o.orderId << " | "
                     << setw(20) << left << o.customerName << " | "
                     << setw(18) << left << statusToString(o.status) << " | "
                     << setw(18) << left << o.paymentMethod << " | "
                     << setw(5)  << right << waitStr << " |\n";
            }
        }

        if (!hasActive) {
            cout << "| No orders currently waiting in the kitchen queue.                                     |\n";
        }
        cout << "+-------+----------+----------------------+--------------------+--------------------+-------+\n";
    }

    bool updateOrderStatus(int tokenNumber, OrderStatus newStatus) {
        Order *o = findOrderByToken(tokenNumber);
        if (o) {
            o->status = newStatus;
            saveToFile();
            return true;
        }
        return false;
    }

    void generateSalesReport() const {
        double totalRevenue = 0.0;
        int completedCount = 0;
        int activeCount = 0;
        int cancelledCount = 0;
        map<string, int> itemFrequency;
        map<string, double> itemSales;

        for (const auto &o : orders) {
            if (o.status == STATUS_CANCELLED) {
                cancelledCount++;
                continue;
            }
            if (o.status == STATUS_COMPLETED) {
                completedCount++;
                totalRevenue += o.totalAmount;
            } else {
                activeCount++;
                totalRevenue += o.totalAmount;
            }

            for (const auto &it : o.items) {
                itemFrequency[it.name] += it.quantity;
                itemSales[it.name] += it.getSubtotal();
            }
        }

        cout << "\n=======================================================\n";
        cout << "               CANTEEN SALES & ANALYTICS REPORT        \n";
        cout << "=======================================================\n";
        cout << "  Total Cumulative Revenue:     Rs. " << fixed << setprecision(2) << totalRevenue << "\n";
        cout << "  Total Orders Received:        " << orders.size() << "\n";
        cout << "  Orders Completed/Delivered:   " << completedCount << "\n";
        cout << "  Orders In Progress (Kitchen): " << activeCount << "\n";
        cout << "  Orders Cancelled:             " << cancelledCount << "\n";
        cout << "-------------------------------------------------------\n";
        cout << "  TOP SELLING MENU ITEMS LEADERBOARD:\n";
        cout << "-------------------------------------------------------\n";

        // Sort items by units sold
        vector<pair<string, int>> sortedItems(itemFrequency.begin(), itemFrequency.end());
        sort(sortedItems.begin(), sortedItems.end(), [](const auto &a, const auto &b) {
            return a.second > b.second;
        });

        if (sortedItems.empty()) {
            cout << "  No items sold yet.\n";
        } else {
            cout << "  " << left << setw(30) << "Item Name" 
                 << setw(12) << "Units Sold" 
                 << "Revenue Generated\n";
            for (size_t i = 0; i < min((size_t)5, sortedItems.size()); ++i) {
                string name = sortedItems[i].first;
                int qty = sortedItems[i].second;
                double rev = itemSales[name];
                cout << "  " << left << setw(30) << name 
                     << setw(12) << qty 
                     << "Rs. " << fixed << setprecision(2) << rev << "\n";
            }
        }
        cout << "=======================================================\n";
    }
};

// ============================================================================
// SHOPPING CART CLASS
// ============================================================================
class SmartCart {
private:
    vector<CartItem> cart;
    string appliedPromoCode;
    double discountPercentage;
    double flatDiscount;
    int redeemedPoints;

public:
    SmartCart() : discountPercentage(0.0), flatDiscount(0.0), redeemedPoints(0) {}

    void clear() {
        cart.clear();
        appliedPromoCode = "";
        discountPercentage = 0.0;
        flatDiscount = 0.0;
        redeemedPoints = 0;
    }

    bool isEmpty() const {
        return cart.empty();
    }

    const vector<CartItem>& getItems() const {
        return cart;
    }

    int getItemQuantity(int itemId) const {
        for (const auto &ci : cart) {
            if (ci.itemId == itemId) return ci.quantity;
        }
        return 0;
    }

    void addItem(const MenuItem &item, int quantity, const string &notes) {
        for (auto &ci : cart) {
            if (ci.itemId == item.id) {
                ci.quantity += quantity;
                if (!notes.empty()) {
                    if (!ci.notes.empty()) ci.notes += ", ";
                    ci.notes += notes;
                }
                return;
            }
        }
        CartItem newItem = {item.id, item.name, item.category, item.price, quantity, notes, item.prepTimeMin};
        cart.push_back(newItem);
    }

    bool removeItem(int itemId) {
        for (auto it = cart.begin(); it != cart.end(); ++it) {
            if (it->itemId == itemId) {
                cart.erase(it);
                return true;
            }
        }
        return false;
    }

    bool updateQuantity(int itemId, int newQty) {
        for (auto &ci : cart) {
            if (ci.itemId == itemId) {
                if (newQty <= 0) {
                    return removeItem(itemId);
                }
                ci.quantity = newQty;
                return true;
            }
        }
        return false;
    }

    double getSubtotal() const {
        double sub = 0.0;
        for (const auto &ci : cart) {
            sub += ci.getSubtotal();
        }
        return sub;
    }

    // Smart combo & promo detection
    bool hasCategory(const string &cat) const {
        for (const auto &ci : cart) {
            if (toUpperCase(ci.category) == toUpperCase(cat)) return true;
        }
        return false;
    }

    bool applyPromo(const string &code) {
        string c = toUpperCase(code);
        double sub = getSubtotal();

        if (c == "STUDENT10") {
            appliedPromoCode = "STUDENT10";
            discountPercentage = 0.10;
            flatDiscount = 0.0;
            return true;
        } else if (c == "FEAST20") {
            if (sub >= 200.0) {
                appliedPromoCode = "FEAST20";
                discountPercentage = 0.20;
                flatDiscount = 0.0;
                return true;
            } else {
                cout << ">> Promo 'FEAST20' requires a minimum subtotal of Rs. 200!\n";
                return false;
            }
        } else if (c == "COMBO25") {
            // Meal + Beverage combo offer
            if (hasCategory("Meals") && hasCategory("Beverages")) {
                appliedPromoCode = "COMBO25";
                discountPercentage = 0.0;
                flatDiscount = 25.0;
                return true;
            } else {
                cout << ">> Promo 'COMBO25' requires at least 1 Meal and 1 Beverage in cart!\n";
                return false;
            }
        }
        return false;
    }

    void setLoyaltyRedemption(int points) {
        redeemedPoints = points;
    }

    double getDiscountAmount() const {
        double sub = getSubtotal();
        double discount = (sub * discountPercentage) + flatDiscount + redeemedPoints;
        if (discount > sub) discount = sub;
        return discount;
    }

    double getTaxAmount() const {
        double taxableAmount = getSubtotal() - getDiscountAmount();
        if (taxableAmount < 0) taxableAmount = 0;
        return taxableAmount * TAX_RATE;
    }

    double getGrandTotal() const {
        double total = (getSubtotal() - getDiscountAmount()) + getTaxAmount();
        return (total < 0) ? 0.0 : total;
    }

    void displayCart() const {
        cout << "\n=========================================================================\n";
        cout << "                           YOUR ORDER CART                               \n";
        cout << "=========================================================================\n";

        if (cart.empty()) {
            cout << ">> Your cart is currently empty!\n";
            cout << "=========================================================================\n";
            return;
        }

        cout << "+-----+-------------------------------+------------+-----+--------------+----------------------+\n";
        cout << "| ID  | Item Name                     | Price (Rs) | Qty | Subtotal(Rs) | Special Notes        |\n";
        cout << "+-----+-------------------------------+------------+-----+--------------+----------------------+\n";

        for (const auto &ci : cart) {
            string noteDisplay = ci.notes.empty() ? "-" : ci.notes;
            if (noteDisplay.length() > 20) noteDisplay = noteDisplay.substr(0, 17) + "...";

            cout << "| " << setw(3) << left << ci.itemId << " | "
                 << setw(29) << left << ci.name << " | "
                 << setw(10) << right << fixed << setprecision(2) << ci.price << " | "
                 << setw(3)  << right << ci.quantity << " | "
                 << setw(12) << right << fixed << setprecision(2) << ci.getSubtotal() << " | "
                 << setw(20) << left << noteDisplay << " |\n";
        }
        cout << "+-----+-------------------------------+------------+-----+--------------+----------------------+\n";

        double sub = getSubtotal();
        double disc = getDiscountAmount();
        double tax = getTaxAmount();
        double total = getGrandTotal();

        cout << right;
        cout << setw(55) << "Subtotal: Rs. " << setw(10) << fixed << setprecision(2) << sub << "\n";
        if (disc > 0) {
            string promoLabel = appliedPromoCode.empty() ? "Discount" : ("Discount (" + appliedPromoCode + ")");
            if (redeemedPoints > 0) promoLabel += " + Loyalty";
            cout << setw(55) << (promoLabel + ": -Rs. ") << setw(10) << fixed << setprecision(2) << disc << "\n";
        }
        cout << setw(55) << "GST / Tax (5%): Rs. " << setw(10) << fixed << setprecision(2) << tax << "\n";
        cout << "-------------------------------------------------------------------------\n";
        cout << setw(55) << "TOTAL PAYABLE: Rs. " << setw(10) << fixed << setprecision(2) << total << "\n";
        cout << "=========================================================================\n";
    }
};

// ============================================================================
// RECEIPT GENERATOR
// ============================================================================
void printOrderSlip(const Order &order) {
    cout << "\n=======================================================\n";
    cout << "             CAMPUS SMART CANTEEN TOKEN SLIP          \n";
    cout << "=======================================================\n";
    cout << "  TOKEN NUMBER: #" << order.tokenNumber << "   [KEEP THIS SLIP]\n";
    cout << "  Order ID:     " << order.orderId << "\n";
    cout << "  Customer:     " << order.customerName;
    if (!order.studentId.empty()) cout << " (" << order.studentId << ")";
    cout << "\n";
    cout << "  Date & Time:  " << order.timestamp << "\n";
    cout << "  Order Status: " << statusToString(order.status) << "\n";
    cout << "  Estimated Prep Time: ~" << order.estimatedTimeMin << " minutes\n";
    cout << "-------------------------------------------------------\n";
    cout << "  " << left << setw(24) << "Item"
         << setw(8)  << "Price"
         << setw(6)  << "Qty"
         << right << setw(12) << "Amount\n";
    cout << "-------------------------------------------------------\n";

    for (const auto &it : order.items) {
        cout << "  " << left << setw(24) << (it.name.length() > 23 ? it.name.substr(0, 20) + "..." : it.name)
             << setw(8)  << fixed << setprecision(2) << it.price
             << setw(6)  << it.quantity
             << right << setw(12) << fixed << setprecision(2) << it.getSubtotal() << "\n";
        if (!it.notes.empty()) {
            cout << "    * Note: " << it.notes << "\n";
        }
    }

    cout << "-------------------------------------------------------\n";
    cout << right;
    cout << "  " << setw(38) << "Subtotal: Rs. " << setw(10) << fixed << setprecision(2) << order.subtotal << "\n";
    if (order.discount > 0) {
        cout << "  " << setw(38) << "Discount: -Rs. " << setw(10) << fixed << setprecision(2) << order.discount << "\n";
    }
    cout << "  " << setw(38) << "Tax (5%): Rs. " << setw(10) << fixed << setprecision(2) << order.tax << "\n";
    cout << "=======================================================\n";
    cout << "  " << setw(38) << "FINAL TOTAL: Rs. " << setw(10) << fixed << setprecision(2) << order.totalAmount << "\n";
    cout << "  " << setw(38) << "Payment Mode: " << setw(10) << order.paymentMethod << "\n";
    cout << "=======================================================\n";
    cout << "  Watch the kitchen display board for your Token #" << order.tokenNumber << "!\n";
    cout << "             Thank you! Have a delicious meal!         \n";
    cout << "=======================================================\n";
}

// ============================================================================
// SYSTEM CONTROLLER / APP
// ============================================================================
class SmartCanteenApp {
private:
    MenuManager menuMgr;
    UserManager userMgr;
    OrderManager orderMgr;
    SmartCart cart;

    UserAccount *currentUser;

    void viewMenuInteractive() {
        while (true) {
            cout << "\n--- CANTEEN MENU BROWSER ---\n";
            cout << "1. Display All Items\n";
            cout << "2. Filter by Category\n";
            cout << "3. Filter Pure Vegetarian Only\n";
            cout << "4. Search by Name/Keyword\n";
            cout << "5. Return to Student Menu\n";
            int choice = getValidInt("Choose filter option (1-5): ", 1, 5);

            if (choice == 1) {
                menuMgr.displayAll();
            } else if (choice == 2) {
                cout << "\nCategories: Breakfast | Meals | Snacks | Beverages | Desserts\n";
                string cat = getValidLine("Enter category name: ");
                menuMgr.displayByCategory(cat);
            } else if (choice == 3) {
                menuMgr.displayVegOnly();
            } else if (choice == 4) {
                string kw = getValidLine("Enter search keyword: ");
                menuMgr.searchByName(kw);
            } else {
                break;
            }
        }
    }

    void addItemToCartInteractive() {
        menuMgr.displayAll();
        int itemId = getValidInt("\nEnter Item ID to add (or 0 to cancel): ", 0, 9999);
        if (itemId == 0) return;

        MenuItem *item = menuMgr.findItemById(itemId);
        if (!item) {
            cout << ">> Error: Item ID not found!\n";
            return;
        }

        if (item->stock <= 0) {
            cout << ">> Sorry, " << item->name << " is currently OUT OF STOCK!\n";
            return;
        }

        int currentInCart = cart.getItemQuantity(itemId);
        int availableToAdd = item->stock - currentInCart;

        if (availableToAdd <= 0) {
            cout << ">> You already have all available " << item->stock << " units of this item in your cart!\n";
            return;
        }

        cout << "Item: " << item->name << " | Price: Rs. " << fixed << setprecision(2) << item->price 
             << " | Available: " << availableToAdd << "\n";
        int qty = getValidInt("Enter quantity to add: ", 1, availableToAdd);

        cout << "Any special instructions? (e.g., extra spicy, no onion, cold) or press Enter to skip: ";
        string notes;
        getline(cin, notes);

        cart.addItem(*item, qty, notes);
        cout << ">> Success: Added " << qty << " x " << item->name << " to cart!\n";
    }

    void manageCartInteractive() {
        while (true) {
            cart.displayCart();
            if (cart.isEmpty()) break;

            cout << "\n--- CART ACTIONS ---\n";
            cout << "1. Modify Item Quantity\n";
            cout << "2. Remove Item from Cart\n";
            cout << "3. Clear Entire Cart\n";
            cout << "4. Apply Promo / Coupon Code\n";
            cout << "5. Proceed to Checkout\n";
            cout << "6. Back to Ordering Menu\n";
            int action = getValidInt("Select action (1-6): ", 1, 6);

            if (action == 1) {
                int id = getValidInt("Enter Item ID to modify: ", 1, 9999);
                MenuItem *mItem = menuMgr.findItemById(id);
                if (!mItem) {
                    cout << ">> Item not found!\n";
                    continue;
                }
                cout << "Current stock available: " << mItem->stock << "\n";
                int newQty = getValidInt("Enter new quantity (0 to remove): ", 0, mItem->stock);
                cart.updateQuantity(id, newQty);
                cout << ">> Cart updated!\n";
            } else if (action == 2) {
                int id = getValidInt("Enter Item ID to remove: ", 1, 9999);
                if (cart.removeItem(id)) {
                    cout << ">> Item removed from cart.\n";
                } else {
                    cout << ">> Item was not in your cart.\n";
                }
            } else if (action == 3) {
                cart.clear();
                cout << ">> Cart cleared.\n";
                break;
            } else if (action == 4) {
                cout << "\nAvailable Promos: \n";
                cout << " - STUDENT10 : 10% Flat Student Discount\n";
                cout << " - FEAST20   : 20% Discount on orders above Rs. 200\n";
                cout << " - COMBO25   : Rs. 25 Flat Off on Meal + Beverage combo\n";
                string code = getValidLine("Enter promo code: ");
                if (cart.applyPromo(code)) {
                    cout << ">> Promo code '" << toUpperCase(code) << "' applied successfully!\n";
                } else {
                    cout << ">> Invalid promo code or conditions not met.\n";
                }
            } else if (action == 5) {
                checkoutInteractive();
                break;
            } else {
                break;
            }
        }
    }

    void checkoutInteractive() {
        if (cart.isEmpty()) {
            cout << ">> Cannot checkout with an empty cart!\n";
            return;
        }

        // Verify stock availability one more time before checkout
        for (const auto &ci : cart.getItems()) {
            MenuItem *m = menuMgr.findItemById(ci.itemId);
            if (!m || m->stock < ci.quantity) {
                cout << ">> Error: Not enough stock for " << ci.name 
                     << "! Only " << (m ? m->stock : 0) << " available. Please adjust your cart.\n";
                return;
            }
        }

        // Check if customer wants to redeem loyalty points
        if (currentUser && currentUser->loyaltyPoints > 0) {
            cout << "\nYou have " << currentUser->loyaltyPoints << " loyalty points! (1 point = Rs. 1)\n";
            int maxUsable = min(currentUser->loyaltyPoints, (int)cart.getSubtotal());
            int usePts = getValidInt("How many points do you want to redeem? (0 to " + to_string(maxUsable) + "): ", 0, maxUsable);
            cart.setLoyaltyRedemption(usePts);
        }

        cart.displayCart();
        double grandTotal = cart.getGrandTotal();

        cout << "\n--- SELECT PAYMENT METHOD ---\n";
        cout << "1. College Smart Card / Digital Wallet\n";
        cout << "2. UPI QR Code Simulation\n";
        cout << "3. Pay Cash at Counter\n";
        cout << "4. Cancel Checkout\n";
        int payChoice = getValidInt("Choose payment option (1-4): ", 1, 4);

        if (payChoice == 4) {
            cout << ">> Checkout cancelled.\n";
            return;
        }

        string payMethod;
        if (payChoice == 1) {
            // Smart Wallet payment
            if (!currentUser) {
                cout << "\n>> You are currently a Guest. Please login to use your Student Wallet.\n";
                string sId = getValidLine("Enter Student ID: ");
                string pin = getValidLine("Enter 4-digit PIN: ");
                UserAccount *acc = userMgr.authenticate(sId, pin);
                if (!acc) {
                    cout << ">> Invalid Student ID or PIN! Payment aborted.\n";
                    return;
                }
                currentUser = acc;
            }

            cout << "Student: " << currentUser->name << " | Current Wallet Balance: Rs. " 
                 << fixed << setprecision(2) << currentUser->walletBalance << "\n";

            if (currentUser->walletBalance < grandTotal) {
                cout << ">> Insufficient balance! (Need Rs. " << grandTotal << ", Available: Rs. " 
                     << currentUser->walletBalance << ")\n";
                cout << "Would you like to recharge your wallet now? (1: Yes, 2: No): ";
                int rcChoice = getValidInt("", 1, 2);
                if (rcChoice == 1) {
                    double rechargeAmt = getValidDouble("Enter recharge amount (Rs): ", 10.0, 5000.0);
                    userMgr.rechargeWallet(currentUser->studentId, rechargeAmt);
                    cout << ">> Wallet recharged! New Balance: Rs. " << currentUser->walletBalance << "\n";
                    if (currentUser->walletBalance < grandTotal) {
                        cout << ">> Balance still insufficient! Order cancelled.\n";
                        return;
                    }
                } else {
                    return;
                }
            }

            // Deduct wallet
            userMgr.deductWallet(currentUser->studentId, grandTotal);
            payMethod = "Smart Wallet (PAID)";
            cout << ">> Payment successful via Student Wallet! Remaining Balance: Rs. " 
                 << fixed << setprecision(2) << currentUser->walletBalance << "\n";

        } else if (payChoice == 2) {
            cout << "\n==========================================\n";
            cout << "        UPI INSTANT SCAN & PAY            \n";
            cout << "==========================================\n";
            cout << "  UPI VPA: canteen@campus.smartbank       \n";
            cout << "  Amount:  Rs. " << fixed << setprecision(2) << grandTotal << "\n";
            cout << "  [ simulated QR code generated ]         \n";
            cout << "==========================================\n";
            cout << "Simulating payment verification...";
            cout << " [SUCCESS]\n";
            payMethod = "UPI Online (PAID)";

        } else {
            payMethod = "Cash at Counter (PENDING)";
            cout << ">> Order placed! Please pay Rs. " << fixed << setprecision(2) 
                 << grandTotal << " at the canteen cash counter to receive your food.\n";
        }

        // Deduct stock from inventory
        for (const auto &ci : cart.getItems()) {
            menuMgr.updateStock(ci.itemId, -ci.quantity);
        }

        // Deduct redeemed loyalty points if any
        if (currentUser && cart.getGrandTotal() > 0) {
            // Points already applied in cart
        }

        // Create Order
        Order newOrder;
        newOrder.orderId = orderMgr.generateNextOrderId();
        newOrder.tokenNumber = orderMgr.generateNextToken();
        newOrder.studentId = currentUser ? currentUser->studentId : "GUEST";
        newOrder.customerName = currentUser ? currentUser->name : "Guest Customer";
        newOrder.timestamp = getCurrentTimestamp();
        newOrder.items = cart.getItems();
        newOrder.subtotal = cart.getSubtotal();
        newOrder.discount = cart.getDiscountAmount();
        newOrder.tax = cart.getTaxAmount();
        newOrder.totalAmount = grandTotal;
        newOrder.paymentMethod = payMethod;
        newOrder.status = STATUS_PLACED;
        newOrder.estimatedTimeMin = orderMgr.calculateEstimatedWaitTime(cart.getItems());

        orderMgr.addOrder(newOrder);

        // Clear cart
        cart.clear();

        // Print physical/digital token slip
        printOrderSlip(newOrder);
        pauseConsole();
    }

    void userWalletInteractive() {
        if (!currentUser) {
            cout << "\nPlease login to view or recharge your wallet.\n";
            string sId = getValidLine("Enter Student ID: ");
            string pin = getValidLine("Enter 4-digit PIN: ");
            UserAccount *acc = userMgr.authenticate(sId, pin);
            if (!acc) {
                cout << ">> Invalid Student ID or PIN!\n";
                return;
            }
            currentUser = acc;
        }

        while (true) {
            cout << "\n=======================================================\n";
            cout << "             SMART CAMPUS DIGITAL WALLET               \n";
            cout << "=======================================================\n";
            cout << "  Student ID:     " << currentUser->studentId << "\n";
            cout << "  Student Name:   " << currentUser->name << "\n";
            cout << "  Wallet Balance: Rs. " << fixed << setprecision(2) << currentUser->walletBalance << "\n";
            cout << "  Loyalty Points: " << currentUser->loyaltyPoints << " pts (Value: Rs. " 
                 << currentUser->loyaltyPoints << ")\n";
            cout << "-------------------------------------------------------\n";
            cout << "1. Recharge Wallet\n";
            cout << "2. View My Order History\n";
            cout << "3. Logout from Wallet\n";
            cout << "4. Return to Main Menu\n";
            int wChoice = getValidInt("Select option (1-4): ", 1, 4);

            if (wChoice == 1) {
                double amt = getValidDouble("Enter recharge amount (Rs. 10 - 5000): ", 10.0, 5000.0);
                userMgr.rechargeWallet(currentUser->studentId, amt);
                cout << ">> Top-up successful! New Balance: Rs. " 
                     << fixed << setprecision(2) << currentUser->walletBalance << "\n";
            } else if (wChoice == 2) {
                vector<Order> myOrders = orderMgr.getOrdersByStudent(currentUser->studentId);
                cout << "\n--- YOUR ORDER HISTORY ---\n";
                if (myOrders.empty()) {
                    cout << ">> No past orders found for this account.\n";
                } else {
                    for (const auto &o : myOrders) {
                        cout << "Order #" << o.orderId << " [Token #" << o.tokenNumber << "] | Date: " << o.timestamp 
                             << " | Total: Rs. " << fixed << setprecision(2) << o.totalAmount 
                             << " | Status: " << statusToString(o.status) << "\n";
                    }
                }
            } else if (wChoice == 3) {
                currentUser = nullptr;
                cout << ">> Logged out.\n";
                break;
            } else {
                break;
            }
        }
    }

public:
    SmartCanteenApp() : currentUser(nullptr) {}

    void studentPortal() {
        while (true) {
            cout << "\n=======================================================\n";
            cout << "             CAMPUS CANTEEN - STUDENT PORTAL          \n";
            cout << "=======================================================\n";
            if (currentUser) {
                cout << " Logged in as: " << currentUser->name << " (" << currentUser->studentId 
                     << ") | Wallet: Rs. " << fixed << setprecision(2) << currentUser->walletBalance << "\n";
                cout << "-------------------------------------------------------\n";
            }
            cout << " 1. Browse Food Menu (Categories & Search)\n";
            cout << " 2. Add Food Item to Cart\n";
            cout << " 3. View / Edit Cart (" << cart.getItems().size() << " items in cart)\n";
            cout << " 4. Proceed to Checkout & Generate Token\n";
            cout << " 5. Check Live Order Status (By Token)\n";
            cout << " 6. Student Wallet & Recharge\n";
            cout << " 7. Switch User / Logout\n";
            cout << " 8. Return to Main Screen\n";
            cout << "=======================================================\n";
            int choice = getValidInt("Enter your choice (1-8): ", 1, 8);

            if (choice == 1) {
                viewMenuInteractive();
            } else if (choice == 2) {
                addItemToCartInteractive();
            } else if (choice == 3) {
                manageCartInteractive();
            } else if (choice == 4) {
                checkoutInteractive();
            } else if (choice == 5) {
                trackOrderInteractive();
            } else if (choice == 6) {
                userWalletInteractive();
            } else if (choice == 7) {
                currentUser = nullptr;
                cout << ">> Logged out. You can now login or continue as Guest.\n";
            } else {
                break;
            }
        }
    }

    void kitchenPortal() {
        while (true) {
            orderMgr.displayKitchenQueue();

            cout << "\n--- KITCHEN ORDER OPERATIONS ---\n";
            cout << "1. Mark Order as 'PREPARING'\n";
            cout << "2. Mark Order as 'READY FOR PICKUP' (Call Token)\n";
            cout << "3. Mark Order as 'COMPLETED / DELIVERED'\n";
            cout << "4. Cancel an Order\n";
            cout << "5. Refresh Kitchen Queue\n";
            cout << "6. Return to Main Menu\n";
            int kChoice = getValidInt("Select kitchen operation (1-6): ", 1, 6);

            if (kChoice >= 1 && kChoice <= 4) {
                int token = getValidInt("Enter Token Number: ", 1, 99999);
                Order *ord = orderMgr.findOrderByToken(token);
                if (!ord) {
                    cout << ">> Order with Token #" << token << " not found!\n";
                    continue;
                }

                if (kChoice == 1) {
                    ord->status = STATUS_PREPARING;
                    orderMgr.saveToFile();
                    cout << ">> Token #" << token << " is now marked as PREPARING in kitchen.\n";
                } else if (kChoice == 2) {
                    ord->status = STATUS_READY;
                    orderMgr.saveToFile();
                    cout << "\n=======================================================\n";
                    cout << " >>> TOKEN #" << token << " IS NOW READY FOR PICKUP! <<<\n";
                    cout << " Customer: " << ord->customerName << "\n";
                    cout << "=======================================================\n";
                } else if (kChoice == 3) {
                    ord->status = STATUS_COMPLETED;
                    orderMgr.saveToFile();
                    cout << ">> Token #" << token << " has been DELIVERED to customer. Order closed.\n";
                } else if (kChoice == 4) {
                    ord->status = STATUS_CANCELLED;
                    // Restore stock
                    for (const auto &ci : ord->items) {
                        menuMgr.updateStock(ci.itemId, ci.quantity);
                    }
                    orderMgr.saveToFile();
                    cout << ">> Token #" << token << " has been CANCELLED. Inventory stock restored.\n";
                }
            } else if (kChoice == 5) {
                continue;
            } else {
                break;
            }
        }
    }

    void adminPortal() {
        string pin = getValidLine("Enter Admin Security PIN: ");
        if (pin != ADMIN_PIN) {
            cout << ">> Access Denied: Incorrect Admin PIN!\n";
            return;
        }

        while (true) {
            cout << "\n=======================================================\n";
            cout << "          CANTEEN MANAGEMENT & ADMIN PORTAL            \n";
            cout << "=======================================================\n";
            cout << " 1. View Menu & Live Inventory Stock\n";
            cout << " 2. Add New Food Item to Menu\n";
            cout << " 3. Edit Item Price\n";
            cout << " 4. Restock Item Inventory\n";
            cout << " 5. Remove Item from Menu\n";
            cout << " 6. Check Low Stock Alerts\n";
            cout << " 7. View Sales Analytics & Revenue Reports\n";
            cout << " 8. Manage Student Accounts & Wallets\n";
            cout << " 9. Return to Main Menu\n";
            cout << "=======================================================\n";
            int aChoice = getValidInt("Enter option (1-9): ", 1, 9);

            if (aChoice == 1) {
                menuMgr.displayAll();
            } else if (aChoice == 2) {
                MenuItem newItem;
                newItem.id = menuMgr.generateNextId();
                cout << "Assigning new Item ID: " << newItem.id << "\n";
                newItem.name = getValidLine("Enter Item Name: ");
                newItem.category = getValidLine("Enter Category (Breakfast/Meals/Snacks/Beverages/Desserts): ");
                newItem.price = getValidDouble("Enter Price (Rs): ", 1.0, 5000.0);
                newItem.stock = getValidInt("Enter Initial Stock Quantity: ", 0, 500);
                newItem.prepTimeMin = getValidInt("Enter Estimated Prep Time (minutes): ", 1, 60);
                int isVegInt = getValidInt("Is this item Vegetarian? (1: Yes, 0: No): ", 0, 1);
                newItem.isVeg = (isVegInt == 1);

                if (menuMgr.addItem(newItem)) {
                    cout << ">> Success: Added '" << newItem.name << "' to canteen menu!\n";
                } else {
                    cout << ">> Error: Failed to add item.\n";
                }
            } else if (aChoice == 3) {
                int id = getValidInt("Enter Item ID to edit price: ", 1, 9999);
                MenuItem *item = menuMgr.findItemById(id);
                if (item) {
                    cout << "Current price for " << item->name << " is Rs. " << item->price << "\n";
                    double newP = getValidDouble("Enter new price (Rs): ", 1.0, 5000.0);
                    menuMgr.editPrice(id, newP);
                    cout << ">> Price updated!\n";
                } else {
                    cout << ">> Item ID not found!\n";
                }
            } else if (aChoice == 4) {
                int id = getValidInt("Enter Item ID to restock: ", 1, 9999);
                MenuItem *item = menuMgr.findItemById(id);
                if (item) {
                    cout << "Current stock of " << item->name << " is " << item->stock << "\n";
                    int added = getValidInt("Enter quantity to add to stock: ", 1, 500);
                    menuMgr.restockItem(id, added);
                    cout << ">> Restocked! New stock count: " << item->stock << "\n";
                } else {
                    cout << ">> Item ID not found!\n";
                }
            } else if (aChoice == 5) {
                int id = getValidInt("Enter Item ID to remove: ", 1, 9999);
                if (menuMgr.removeItem(id)) {
                    cout << ">> Item removed successfully from the menu.\n";
                } else {
                    cout << ">> Item ID not found!\n";
                }
            } else if (aChoice == 6) {
                menuMgr.displayLowStockAlerts(5);
            } else if (aChoice == 7) {
                orderMgr.generateSalesReport();
            } else if (aChoice == 8) {
                adminUserManagement();
            } else {
                break;
            }
        }
    }

    void adminUserManagement() {
        while (true) {
            cout << "\n--- STUDENT ACCOUNTS & WALLET MANAGEMENT ---\n";
            cout << "1. List All Registered Students\n";
            cout << "2. Register New Student Card\n";
            cout << "3. Credit / Top-Up Student Wallet\n";
            cout << "4. Back to Admin Portal\n";
            int uChoice = getValidInt("Select option (1-4): ", 1, 4);

            if (uChoice == 1) {
                userMgr.displayAllUsers();
            } else if (uChoice == 2) {
                string id = getValidLine("Enter Student ID (e.g. S105): ");
                string name = getValidLine("Enter Student Name: ");
                string pin = getValidLine("Set 4-digit PIN: ");
                double initBal = getValidDouble("Enter Initial Wallet Balance (Rs): ", 0.0, 5000.0);

                if (userMgr.registerUser(id, name, pin, initBal)) {
                    cout << ">> Student " << name << " registered successfully!\n";
                } else {
                    cout << ">> Error: Student ID already exists!\n";
                }
            } else if (uChoice == 3) {
                string id = getValidLine("Enter Student ID to recharge: ");
                UserAccount *u = userMgr.findUser(id);
                if (u) {
                    cout << "Student: " << u->name << " | Current Balance: Rs. " << u->walletBalance << "\n";
                    double amt = getValidDouble("Enter amount to credit (Rs): ", 1.0, 5000.0);
                    userMgr.rechargeWallet(id, amt);
                    cout << ">> Wallet credited successfully! New Balance: Rs. " << u->walletBalance << "\n";
                } else {
                    cout << ">> Student ID not found!\n";
                }
            } else {
                break;
            }
        }
    }

    void trackOrderInteractive() {
        int token = getValidInt("\nEnter your Token Number to track: ", 1, 99999);
        Order *ord = orderMgr.findOrderByToken(token);

        if (!ord) {
            cout << ">> No order found with Token #" << token << "!\n";
            return;
        }

        cout << "\n=======================================================\n";
        cout << "                  LIVE ORDER STATUS TRACKER            \n";
        cout << "=======================================================\n";
        cout << "  Token Number: #" << ord->tokenNumber << "\n";
        cout << "  Order ID:     " << ord->orderId << "\n";
        cout << "  Customer:     " << ord->customerName << "\n";
        cout << "  Status:       [" << statusToString(ord->status) << "]\n";
        cout << "  Ordered At:   " << ord->timestamp << "\n";
        cout << "  Total Amount: Rs. " << fixed << setprecision(2) << ord->totalAmount << "\n";
        cout << "  Payment:      " << ord->paymentMethod << "\n";
        cout << "-------------------------------------------------------\n";

        if (ord->status == STATUS_PLACED) {
            cout << "  Progress: [###                 ] Placed in queue.\n";
            cout << "  Estimated wait time: ~" << ord->estimatedTimeMin << " minutes.\n";
        } else if (ord->status == STATUS_PREPARING) {
            cout << "  Progress: [########            ] Chef is cooking your food!\n";
            cout << "  Estimated wait time: ~" << max(2, ord->estimatedTimeMin / 2) << " minutes.\n";
        } else if (ord->status == STATUS_READY) {
            cout << "  Progress: [####################] FOOD IS READY!\n";
            cout << "  >> PLEASE PROCEED TO COUNTER TO COLLECT YOUR ORDER! <<\n";
        } else if (ord->status == STATUS_COMPLETED) {
            cout << "  Progress: [COMPLETED] Order delivered. Enjoy your meal!\n";
        } else if (ord->status == STATUS_CANCELLED) {
            cout << "  Progress: [CANCELLED] This order was cancelled.\n";
        }
        cout << "=======================================================\n";
    }

    void run() {
        while (true) {
            cout << "\n=======================================================\n";
            cout << "     WELCOME TO SMART CAMPUS CANTEEN SYSTEM           \n";
            cout << "=======================================================\n";
            cout << "  1. Student / Customer Ordering Portal\n";
            cout << "  2. Kitchen Display System (KDS Queue)\n";
            cout << "  3. Live Order Status Tracker (Lookup Token)\n";
            cout << "  4. Canteen Admin & Inventory Management\n";
            cout << "  5. Exit System\n";
            cout << "=======================================================\n";
            int choice = getValidInt("Please select an option (1-5): ", 1, 5);

            if (choice == 1) {
                studentPortal();
            } else if (choice == 2) {
                kitchenPortal();
            } else if (choice == 3) {
                trackOrderInteractive();
            } else if (choice == 4) {
                adminPortal();
            } else if (choice == 5) {
                cout << "\nThank you for using Smart Campus Canteen System. Goodbye!\n";
                break;
            }
        }
    }
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main() {
    // Instantiate and execute the smart canteen system
    SmartCanteenApp app;
    app.run();
    return 0;
}
