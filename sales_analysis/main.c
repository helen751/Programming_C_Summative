#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "../utils.h"

typedef struct {
    int productId;
    char productName[100];
    double unitPrice;
} Product;

typedef struct {
    int transactionId;
    char productName[100];
    int quantity;
    double unitPrice;
    double totalAmount;
    char transactionDate[11];
} Transaction;

typedef struct {
    double amount;
    int quantity;
    char productName[100];
} FilterCriteria;

typedef bool (*FilterCallback)(
    const Transaction *transaction,
    const FilterCriteria *criteria
);

Transaction *transactions = NULL;
int transaction_count = 0;

#define MAX_PRODUCTS 100

// checking if the transaction ID already exists
int find_transaction(int transaction_id) {
    int index;

    for (index = 0; index < transaction_count; index++) {
        if (transactions[index].transactionId == transaction_id) {
            return (index);
        }
    }
    return (-1);
}

// checking if the total is above the selected amount
bool is_above_amount(const Transaction *transaction,
    const FilterCriteria *criteria) {
    return (transaction->totalAmount > criteria->amount);
}

// checking if the total is below the selected amount
bool is_below_amount(const Transaction *transaction,
    const FilterCriteria *criteria) {
    return (transaction->totalAmount < criteria->amount);
}

// checking if the quantity meets the selected minimum
bool has_minimum_quantity(const Transaction *transaction,
    const FilterCriteria *criteria) {
    return (transaction->quantity >= criteria->quantity);
}

// checking if the transaction contains the selected product
bool matches_product(
    const Transaction *transaction,
    const FilterCriteria *criteria
) {
    return (
        strcmp(
            transaction->productName,
            criteria->productName
        ) == 0
    );
}

// loading the products file
int load_products(Product product_list[]) {
    FILE *file;
    int product_count = 0;

    file = fopen("products.txt", "r");

    if (file == NULL) {
        display_error("Unable to open products.txt.");
        return (0);
    }

    while (
        product_count < MAX_PRODUCTS &&
        fscanf(
            file,
            "%d|%99[^|]|%lf\n",
            &product_list[product_count].productId,
            product_list[product_count].productName,
            &product_list[product_count].unitPrice
        ) == 3) {
        product_count++;
    }
    fclose(file);
    return (product_count);
}

// loading previously saved transactions
int load_transactions(void) {
    FILE *file;
    char line[350];
    Transaction loaded_transaction;
    Transaction *resized_transactions;

    file = fopen("transactions.txt", "r");

    // continuing normally when no transaction file exists yet
    if (file == NULL) {
        display_warning("No saved transactions were found.");
        return (1);
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(
            line,
            "%d|%99[^|]|%d|%lf|%lf|%10[^\n]",
            &loaded_transaction.transactionId,
            loaded_transaction.productName,
            &loaded_transaction.quantity,
            &loaded_transaction.unitPrice,
            &loaded_transaction.totalAmount,
            loaded_transaction.transactionDate
        ) != 6) {
            continue;
        }

        if (transactions == NULL) {
            resized_transactions = malloc(sizeof(Transaction));
        } else {
            resized_transactions = realloc(
                transactions,
                (transaction_count + 1) * sizeof(Transaction)
            );
        }

        if (resized_transactions == NULL) {
            display_error("Failed to allocate memory for saved transactions.");
            fclose(file);
            return (0);
        }

        transactions = resized_transactions;
        transactions[transaction_count] = loaded_transaction;
        transaction_count++;
    }
    fclose(file);

    printf(GREEN "\n%d saved transaction(s) loaded.\n" RESET,transaction_count);

    return (1);
}

// saving all transactions to the transaction file
int save_transactions(void) {
    FILE *file;
    int index;

    file = fopen("transactions.txt", "w");

    if (file == NULL) {
        perror(RED"Unable to save transactions."RESET);
        return (0);
    }

    for (index = 0; index < transaction_count; index++) {
        fprintf(
            file,
            "%d|%s|%d|%.2f|%.2f|%s\n",
            transactions[index].transactionId,
            transactions[index].productName,
            transactions[index].quantity,
            transactions[index].unitPrice,
            transactions[index].totalAmount,
            transactions[index].transactionDate
        );
    }
    fclose(file);

    return (1);
}

// adding a new sales transaction
void add_transaction(void) {
    clear_console();

    int transaction_id;
    int product_choice;
    int quantity;
    int index;
    Product product_list[MAX_PRODUCTS];
    Product *selected_product;
    int product_count;

    Transaction *new_transaction;
    Transaction *resized_transactions;
    time_t current_time;
    struct tm *date_information;

    printf("\n\t======== ADD NEW TRANSACTION ========\n");

    // loading the products
    product_count = load_products(product_list);

    if (product_count == 0) {
        display_error("No products are available.");
        return;
    }

    printf("\n\tAVAILABLE PRODUCTS \n");

    for (index = 0; index < product_count; index++) {
        printf(
            "%d. %-25s %.2f RWF\n",
            index + 1,
            product_list[index].productName,
            product_list[index].unitPrice
        );
    }

    if (!read_int("\nSelect a product: ", &product_choice)) {
        return;
    }

    if (product_choice < 1 || product_choice > product_count) {
        display_error("The selected product does not exist.");
        return;
    }

    selected_product = &product_list[product_choice - 1];

    printf("\nSelected Product: %s\n", selected_product->productName);
    printf("Unit Price: %.2f RWF\n\n", selected_product->unitPrice);

    if (!read_int("Enter new transaction ID: ", &transaction_id)) {
        return;
    }

    if (transaction_id <= 0) {
        display_error("Transaction ID must be greater than zero.");
        return;
    }

    if (find_transaction(transaction_id) != -1) {
        display_error("A transaction with this ID already exists.");
        return;
    }

    if (!read_int("Enter quantity: ", &quantity)) {
        return;
    }

    if (quantity <= 0) {
        display_error("Quantity must be greater than zero.");
        return;
    }

    // increasing the transaction memory by one record
    if (transactions == NULL) {
        resized_transactions = malloc(sizeof(Transaction));
    } else {
        resized_transactions = realloc(
            transactions,
            (transaction_count + 1) * sizeof(Transaction)
        );
    }

    if (resized_transactions == NULL) {
        display_error("Failed to allocate memory for the transaction.");
        return;
    }

    transactions = resized_transactions;
    new_transaction = &transactions[transaction_count];

    new_transaction->transactionId = transaction_id;

    snprintf(
        new_transaction->productName,
        sizeof(new_transaction->productName),
        "%s",
        selected_product->productName
    );

    new_transaction->quantity = quantity;
    new_transaction->unitPrice = selected_product->unitPrice;
    new_transaction->totalAmount = selected_product->unitPrice * quantity;

    // recording the current date automatically
    current_time = time(NULL);
    date_information = localtime(&current_time);

    if (date_information != NULL) {
        strftime(
            new_transaction->transactionDate,
            sizeof(new_transaction->transactionDate),
            "%d/%m/%Y",
            date_information
        );
    } else {
        snprintf(
            new_transaction->transactionDate,
            sizeof(new_transaction->transactionDate),
            "Unknown"
        );
    }

    transaction_count++;
    printf(YELLOW"\t\tProcessing transaction...\n"RESET);
    sleep(1);
    if (!save_transactions()) {
        display_warning("Failed to save transactions to file.");
        transaction_count--;
        return;
    }
    display_success("\t\tTransaction added successfully.\n");
    printf("\t\tProduct: %s\n", new_transaction->productName);
    printf("\t\tUnit Price: %.2f RWF\n", new_transaction->unitPrice);
    printf("\t\tQuantity: %d\n", new_transaction->quantity);
    printf("\t\tTotal Amount: %.2f RWF\n", new_transaction->totalAmount);
    printf("\t\tTransaction Date: %s\n", new_transaction->transactionDate);

    wait_for_enter();
    clear_console();
}

// displaying a transaction list in a table
void display_transaction_table(
    const Transaction transaction_list[],
    int list_count,
    const char *title) {
    int index;

    printf(CYAN "\n\t\t======== %s ========\n\n" RESET, title);

    printf("+------+----------------+---------------------------+----------+"
        "------------------+------------------+------------+\n");

    printf(
        "| %-4s | %-14s | %-25s | %-8s | %-16s | %-16s | %-10s |\n",
        "No.",
        "Transaction ID",
        "Product",
        "Quantity",
        "Unit Price (RWF)",
        "Total (RWF)",
        "Date"
    );

    printf("+------+----------------+---------------------------+----------+"
        "------------------+------------------+------------+\n");

    for (index = 0; index < list_count; index++) {
        printf(
            "| %-4d | %-14d | %-25.25s | %8d | %16.2f | %16.2f | %-10s |\n",
            index + 1,
            transaction_list[index].transactionId,
            transaction_list[index].productName,
            transaction_list[index].quantity,
            transaction_list[index].unitPrice,
            transaction_list[index].totalAmount,
            transaction_list[index].transactionDate
        );
    }

    printf("+------+----------------+---------------------------+----------+"
        "------------------+------------------+------------+\n");

    printf(GREEN "\nTotal Transactions: %d\n" RESET,list_count);
}

// displaying all saved sales transactions
void display_transactions(void) {
    clear_console();

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_transaction_table(
        transactions,
        transaction_count,
        "ALL SALES TRANSACTIONS"
    );

    wait_for_enter();
    clear_console();
}

// updating an existing transaction
void update_transaction(void) {
    Product product_list[MAX_PRODUCTS];
    Product *selected_product;
    Transaction *transaction;
    int transaction_id;
    int transaction_index;
    int product_count;
    int product_choice;
    int new_quantity;
    int index;

    clear_console();

    printf(CYAN "\n\t======== UPDATE EXISTING TRANSACTION ========\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        return;
    }

    if (!read_int("Enter the transaction ID to update: ", &transaction_id)) {
        return;
    }

    transaction_index = find_transaction(transaction_id);

    if (transaction_index == -1) {
        display_error("The transaction does not exist.");
        return;
    }

    transaction = &transactions[transaction_index];

    printf(YELLOW"\nCurrent Transaction\n"RESET);
    printf("Product: %s\n", transaction->productName);
    printf("Quantity: %d\n", transaction->quantity);
    printf("Unit Price: %.2f RWF\n", transaction->unitPrice);
    printf("Total Amount: %.2f RWF\n", transaction->totalAmount);
    printf("Date: %s\n", transaction->transactionDate);

    // loading products for the new selection
    product_count = load_products(product_list);

    if (product_count == 0) {
        display_error("No products are available.");
        return;
    }

    printf(CYAN"\n\t\tAvailable Products\n\n"RESET);

    for (index = 0; index < product_count; index++) {
        printf("\t%d. %-25s %.2f RWF\n",
            index + 1,
            product_list[index].productName,
            product_list[index].unitPrice
        );
    }

    if (!read_int("\n\tSelect the new product: ", &product_choice)) {
        return;
    }

    if (product_choice < 1 || product_choice > product_count) {
        display_error("The selected product does not exist.");
        return;
    }

    if (!read_int("\tEnter the new quantity: ", &new_quantity)) {
        return;
    }

    if (new_quantity <= 0) {
        display_error("Quantity must be greater than zero.");
        return;
    }

    selected_product = &product_list[product_choice - 1];

    snprintf(
        transaction->productName,
        sizeof(transaction->productName),
        "%s",
        selected_product->productName
    );

    transaction->quantity = new_quantity;
    transaction->unitPrice = selected_product->unitPrice;
    transaction->totalAmount =
        selected_product->unitPrice * new_quantity;

    if (!save_transactions()) {
        display_warning("The updated transaction could not be saved.");
        return;
    }

    display_success("Transaction updated successfully.");

    printf("\n\t\tProduct: %s\n\t\t", transaction->productName);
    printf("Quantity: %d\n\t\t", transaction->quantity);
    printf("Unit Price: %.2f RWF\n\t\t", transaction->unitPrice);
    printf("Total Amount: %.2f RWF\n\t\t", transaction->totalAmount);
    printf("Date: %s\n", transaction->transactionDate);

    wait_for_enter();
    clear_console();
}

// deleting an existing transaction
void delete_transaction(void) {
    Transaction *transaction;
    char confirmation[10];
    int transaction_id;
    int transaction_index;
    int index;

    clear_console();

    printf(CYAN "\n\t======== DELETE TRANSACTION ========\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        return;
    }

    if (!read_int("\t\tEnter the transaction ID to delete: ", &transaction_id)) {
        return;
    }

    transaction_index = find_transaction(transaction_id);

    if (transaction_index == -1) {
        display_error("The transaction does not exist.");
        return;
    }

    transaction = &transactions[transaction_index];

    printf("\n\tID: %d\n\t", transaction->transactionId);
    printf("Product: %s\n\t", transaction->productName);
    printf("Quantity: %d\n\t", transaction->quantity);
    printf("Total Amount: %.2f RWF\n", transaction->totalAmount);

    if (!read_text("\nDelete this transaction? (y/n): ",
        confirmation,
        sizeof(confirmation))) {
        return;
    }

    if (confirmation[0] != 'y' && confirmation[0] != 'Y') {
        display_warning("Transaction deletion cancelled.");
        return;
    }

    // moving the remaining transactions backward
    for (index = transaction_index; index < transaction_count - 1; index++) {
        transactions[index] = transactions[index + 1];
    }

    transaction_count--;

    if (!save_transactions()) {
        display_warning("The transaction file could not be updated.");
        return;
    }

    display_success("Transaction deleted successfully.");
    wait_for_enter();
    clear_console();
}

// applying the callback and displaying matching transactions
void display_filtered_transactions(
    FilterCallback callback,
    const FilterCriteria *criteria) {
        Transaction *matching_transactions;
        int match_count = 0;
        int index;

        matching_transactions = malloc(transaction_count * sizeof(Transaction));

        if (matching_transactions == NULL) {
            display_error("Failed to allocate memory for filter results.");
            return;
        }

        for (index = 0; index < transaction_count; index++) {
            // calling the selected callback
            if (callback(&transactions[index], criteria)) {
                matching_transactions[match_count] = transactions[index];
                match_count++;
            }
        }

        if (match_count == 0) {
            display_warning("No transactions matched the selected filter.");
        } else {
            display_transaction_table(
                matching_transactions,
                match_count,
                "FILTER RESULTS"
            );
        }

    free(matching_transactions);
}

// filtering transactions using callbacks
void filter_transactions(void) {
    Product product_list[MAX_PRODUCTS];
    FilterCriteria criteria = {0};
    FilterCallback callback;
    int product_count;
    int product_choice;
    int choice;
    int index;

    clear_console();

    printf(CYAN "\n\t======== FILTER TRANSACTIONS ========\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        return;
    }

    printf("\t1. Transactions Above an Amount\n");
    printf("\t2. Transactions Below an Amount\n");
    printf("\t3. Transactions With a Minimum Quantity\n");
    printf("\t4. Transactions for a Specific Product\n");
    printf("\t0. Back to Main Menu\n");

    if (!read_int("\n\tSelect a filter: ", &choice)) {
        return;
    }

    if (choice == 0) {
        return;
    }

    if (choice < 1 || choice > 4) {
        display_error("Select a filter between 1 and 4.");
        return;
    }

    if (choice == 1 || choice == 2) {
        if (!read_double("\n\tEnter the transaction amount: ",&criteria.amount)) {
            return;
        }

        if (criteria.amount < 0) {
            display_error("The amount cannot be negative.");
            return;
        }

        if (choice == 1) {
            callback = is_above_amount;
        } else {
            callback = is_below_amount;
        }
    } else if (choice == 3) {
        if (!read_int("\n\tEnter the minimum quantity: ",&criteria.quantity)) {
            return;
        }

        if (criteria.quantity <= 0) {
            display_error("Quantity must be greater than zero.");
            return;
        }

        callback = has_minimum_quantity;
    } else {
        product_count = load_products(product_list);

        if (product_count == 0) {
            display_error("No products are available.");
            return;
        }

        printf(CYAN "\n\tAvailable Products\n\n" RESET);

        for (index = 0; index < product_count; index++) {
            printf(
                "\t%d. %-25s %.2f RWF\n",
                index + 1,
                product_list[index].productName,
                product_list[index].unitPrice
            );
        }

        if (!read_int("\n\tSelect a product: ",&product_choice)) {
            return;
        }

        if (product_choice < 1 || product_choice > product_count) {
            display_error("The selected product does not exist.");
            return;
        }

        snprintf(
            criteria.productName,
            sizeof(criteria.productName),
            "%s",
            product_list[product_choice - 1].productName
        );

        callback = matches_product;
    }

    display_filtered_transactions(callback, &criteria);

    wait_for_enter();
    clear_console();
}

// checking if text contains a search value without case sensitivity
int contains_text(const char *text, const char *search_value) {
    int text_index;
    int search_index;

    for (text_index = 0; text[text_index] != '\0'; text_index++) {
        search_index = 0;

        while (
            search_value[search_index] != '\0' &&
            text[text_index + search_index] != '\0' &&
            tolower((unsigned char)text[text_index + search_index]) ==
            tolower((unsigned char)search_value[search_index])
        ) {
            search_index++;
        }

        if (search_value[search_index] == '\0') {
            return (1);
        }
    }

    return (0);
}

// searching across all transaction information
void search_transactions(void) {
    Transaction *matching_transactions;
    char searchable_text[350];
    char search_value[100];
    int match_count = 0;
    int index;

    clear_console();

    printf(CYAN "\n\t======== SEARCH TRANSACTIONS ========\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        wait_for_enter();
        clear_console();
        return;
    }

    if (!read_text(
        "Enter an ID, product, quantity, amount, or date: ",
        search_value,
        sizeof(search_value)
    )) {
        return;
    }

    matching_transactions = malloc(
        transaction_count * sizeof(Transaction)
    );

    if (matching_transactions == NULL) {
        display_error("Failed to allocate memory for search results.");
        return;
    }

    for (index = 0; index < transaction_count; index++) {
        snprintf(
            searchable_text,
            sizeof(searchable_text),
            "%d %s %d %.2f %.2f %s",
            transactions[index].transactionId,
            transactions[index].productName,
            transactions[index].quantity,
            transactions[index].unitPrice,
            transactions[index].totalAmount,
            transactions[index].transactionDate
        );

        if (contains_text(searchable_text, search_value)) {
            matching_transactions[match_count] = transactions[index];
            match_count++;
        }
    }

    printf(YELLOW "\nSearching transactions..." RESET);
    fflush(stdout);
    sleep(1);
    clear_console();

    if (match_count == 0) {
        display_warning("No matching transactions were found.");
    } else {
        display_transaction_table(
            matching_transactions,
            match_count,
            "SEARCH RESULTS"
        );
    }

    free(matching_transactions);

    wait_for_enter();
    clear_console();
}

// sorting the transactions
void sort_transactions(void) {
    Transaction temporary_transaction;
    int should_swap;
    int choice;
    int first;
    int second;

    clear_console();

    printf(CYAN "\n\t===== SORT TRANSACTIONS =====\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        wait_for_enter();
        clear_console();
        return;
    }

    printf("\t1. Transaction ID (Ascending)\n");
    printf("\t2. Product Name (Alphabetically)\n");
    printf("\t3. Quantity (Ascending)\n");
    printf("\t4. Total Amount (Lowest to Highest)\n");
    printf("\t5. Total Amount (Highest to Lowest)\n");
    printf("\t0. Back to Main Menu\n");

    if (!read_int("\n\tSelect a sorting option: ", &choice)) {
        return;
    }

    if (choice == 0) {
        return;
    }

    if (choice < 1 || choice > 5) {
        display_error("Select a sorting option between 1 and 5.");
        wait_for_enter();
        clear_console();
        return;
    }

    for (first = 0; first < transaction_count - 1; first++) {
        for (
            second = 0;
            second < transaction_count - first - 1;
            second++
        ) {
            should_swap = 0;

            if (
                choice == 1 &&
                transactions[second].transactionId >
                transactions[second + 1].transactionId
            ) {
                should_swap = 1;
            } else if (
                choice == 2 &&
                strcmp(
                    transactions[second].productName,
                    transactions[second + 1].productName
                ) > 0
            ) {
                should_swap = 1;
            } else if (
                choice == 3 &&
                transactions[second].quantity >
                transactions[second + 1].quantity
            ) {
                should_swap = 1;
            } else if (
                choice == 4 &&
                transactions[second].totalAmount >
                transactions[second + 1].totalAmount
            ) {
                should_swap = 1;
            } else if (
                choice == 5 &&
                transactions[second].totalAmount <
                transactions[second + 1].totalAmount
            ) {
                should_swap = 1;
            }

            if (should_swap) {
                temporary_transaction = transactions[second];
                transactions[second] = transactions[second + 1];
                transactions[second + 1] = temporary_transaction;
            }
        }
    }

    printf(YELLOW "\n\tSorting transactions..." RESET);
    fflush(stdout);
    sleep(1);
    clear_console();

    display_transaction_table(transactions,transaction_count,"SORTED TRANSACTIONS");

    wait_for_enter();
    clear_console();
}

// generating the complete sales report
void generate_sales_report(void) {
    Transaction *highest_transaction, *lowest_transaction;
    time_t current_time;
    struct tm *date_information;
    char current_month_name[30] = "Current Month", best_selling_date[11] = "";
    double total_revenue = 0, average_transaction, product_revenue, daily_revenue, best_daily_revenue = 0;
    int total_quantity = 0, sales_days_this_month = 0, current_month = 0, current_year = 0,
    transaction_day, transaction_month, transaction_year, already_counted, index, second, previous;

    clear_console();

    printf(CYAN "\n\t======== SALES REPORT ========\n\n" RESET);

    if (transaction_count == 0) {
        display_warning("No transactions are currently available.");
        wait_for_enter();
        clear_console();
        return;
    }

    printf(YELLOW "\tGenerating sales report..." RESET);
    fflush(stdout);
    sleep(1);
    clear_console();

    highest_transaction = &transactions[0];
    lowest_transaction = &transactions[0];

    // calculating the main sales values
    for (index = 0; index < transaction_count; index++) {
        total_revenue += transactions[index].totalAmount;
        total_quantity += transactions[index].quantity;

        if (
            transactions[index].totalAmount >
            highest_transaction->totalAmount
        ) {
            highest_transaction = &transactions[index];
        }

        if (
            transactions[index].totalAmount <
            lowest_transaction->totalAmount
        ) {
            lowest_transaction = &transactions[index];
        }
    }

    average_transaction = total_revenue / transaction_count;

    // getting the current month and year
    current_time = time(NULL);
    date_information = localtime(&current_time);

    if (date_information != NULL) {
        current_month = date_information->tm_mon + 1;
        current_year = date_information->tm_year + 1900;

        strftime(
            current_month_name,
            sizeof(current_month_name),
            "%B %Y",
            date_information
        );
    }

    // finding unique sales dates and the best selling date
    for (index = 0; index < transaction_count; index++) {
        already_counted = 0;

        for (previous = 0; previous < index; previous++) {
            if (
                strcmp(
                    transactions[index].transactionDate,
                    transactions[previous].transactionDate
                ) == 0
            ) {
                already_counted = 1;
                break;
            }
        }

        if (already_counted) {
            continue;
        }

        daily_revenue = 0;

        for (second = 0; second < transaction_count; second++) {
            if (
                strcmp(
                    transactions[index].transactionDate,
                    transactions[second].transactionDate
                ) == 0
            ) {
                daily_revenue += transactions[second].totalAmount;
            }
        }

        if (daily_revenue > best_daily_revenue) {
            best_daily_revenue = daily_revenue;

            snprintf(
                best_selling_date,
                sizeof(best_selling_date),
                "%s",
                transactions[index].transactionDate
            );
        }

        if (
            sscanf(
                transactions[index].transactionDate,
                "%d/%d/%d",
                &transaction_day,
                &transaction_month,
                &transaction_year
            ) == 3 &&
            transaction_month == current_month &&
            transaction_year == current_year
        ) {
            sales_days_this_month++;
        }
    }

    printf(CYAN "\n\t======== SALES SUMMARY ========\n\n" RESET);

    printf("%-34s %d\n", "Total Transactions:", transaction_count);
    printf("%-34s %.2f RWF\n", "Total Revenue:", total_revenue);
    printf("%-34s %.2f RWF\n", "Average Transaction Value:", average_transaction);
    printf("%-34s %d\n", "Total Quantity Sold:", total_quantity);
    printf("%s%s%-8s %d\n\n","Sales Days (",current_month_name, "): ", sales_days_this_month);

    printf("%-34s %.2f RWF (%s)\n","Highest Transaction:",highest_transaction->totalAmount, highest_transaction->productName);

    printf(
        "%-34s %.2f RWF (%s)\n",
        "Lowest Transaction:",
        lowest_transaction->totalAmount, lowest_transaction->productName
    );

    printf(CYAN "\n\t=== REVENUE BY PRODUCT ===\n\n" RESET);

    printf("+--------------------------------+------------------+\n");
    printf("| %-30s | %-16s |\n", "Product", " Revenue (RWF)");
    printf("+--------------------------------+------------------+\n");

    // displaying revenue for each product with recorded sales
    for (index = 0; index < transaction_count; index++) {
        already_counted = 0;

        for (previous = 0; previous < index; previous++) {
            if (strcmp(transactions[index].productName,transactions[previous].productName) == 0) {
                already_counted = 1;
                break;
            }
        }

        if (already_counted) {
            continue;
        }

        product_revenue = 0;

        for (second = 0; second < transaction_count; second++) {
            if (strcmp(transactions[index].productName,transactions[second].productName) == 0) {
                product_revenue += transactions[second].totalAmount;
            }
        }

        if (product_revenue > 0) {
            printf(
                "| %-30.30s | %16.2f |\n",
                transactions[index].productName,
                product_revenue
            );
        }
    }

    printf("+--------------------------------+------------------+\n");

    printf(CYAN "\n\t=== BEST SELLING DATE ===\n" RESET);

    printf("%-34s %s\n", "Date:", best_selling_date);
    printf("%-34s %.2f RWF\n", "Revenue for That Date:", best_daily_revenue);

    printf(GREEN "\nSales report generated successfully.\n" RESET);

    wait_for_enter();
    clear_console();
}

// displaying the available products
void display_products(void) {
    Product product_list[MAX_PRODUCTS];
    int product_count;
    int index;

    clear_console();

    printf(CYAN "\n\t======== AVAILABLE PRODUCTS ========\n\n" RESET);

    product_count = load_products(product_list);

    if (product_count == 0) {
        display_warning("No products are currently available.");
        wait_for_enter();
        clear_console();
        return;
    }

    printf("+------+--------------------------------+------------------+\n");
    printf("| %-4s | %-30s | %-16s |\n", "ID", "Product", "Price (RWF)");
    printf("+------+--------------------------------+------------------+\n");

    for (index = 0; index < product_count; index++) {
        printf(
            "| %-4d | %-30.30s | %16.2f |\n",
            product_list[index].productId,
            product_list[index].productName,
            product_list[index].unitPrice
        );
    }

    printf("+------+--------------------------------+------------------+\n");

    printf(GREEN "\nTotal Products: %d\n" RESET,product_count);

    wait_for_enter();
    clear_console();
}

// adding a new product to the product file
void add_product(void) {
    Product product_list[MAX_PRODUCTS];
    char product_name[100];
    double unit_price;
    int product_count;
    int new_product_id = 1;
    int index;
    FILE *file;
    long file_size;
    int last_character;

    clear_console();

    printf(CYAN "\n\t======== ADD NEW PRODUCT ========\n\n" RESET);

    product_count = load_products(product_list);

    if (product_count >= MAX_PRODUCTS) {
        display_error("The maximum number of products has been reached.");
        wait_for_enter();
        clear_console();
        return;
    }

    // finding the next product ID
    for (index = 0; index < product_count; index++) {
        if (product_list[index].productId >= new_product_id) {
            new_product_id = product_list[index].productId + 1;
        }
    }

    if (!read_text("Enter the product name: ",product_name,sizeof(product_name))) {
        return;
    }

    if (strchr(product_name, '|') != NULL) {
        display_error("The product name cannot contain the | character.");
        wait_for_enter();
        clear_console();
        return;
    }

    // checking if the product name already exists
    for (index = 0; index < product_count; index++) {
        if (
            strcmp(
                product_list[index].productName,
                product_name
            ) == 0
        ) {
            display_error("A product with this name already exists.");
            wait_for_enter();
            clear_console();
            return;
        }
    }

    if (!read_double("Enter the unit price: ", &unit_price)) {
        return;
    }

    if (unit_price <= 0) {
        display_error("The unit price must be greater than zero.");
        wait_for_enter();
        clear_console();
        return;
    }

    file = fopen("products.txt", "a+");

    if (file == NULL) {
        perror(RED"Unable to open products.txt."RESET);
        wait_for_enter();
        clear_console();
        return;
    }

    // checking whether the existing file ends with a new line
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);

    if (file_size > 0) {
        fseek(file, -1, SEEK_END);
        last_character = fgetc(file);

        if (last_character != '\n') {
            fseek(file, 0, SEEK_END);
            fputc('\n', file);
        }
    }
    fseek(file, 0, SEEK_END);
    fprintf(file,"%d|%s|%.2f\n",new_product_id,product_name,unit_price);

    fclose(file);

    printf(YELLOW "\nAdding product..." RESET);
    fflush(stdout);
    sleep(1);

    display_success("Product added successfully.");

    printf("\nProduct ID: %d\n", new_product_id);
    printf("Product Name: %s\n", product_name);
    printf("Unit Price: %.2f RWF\n", unit_price);

    wait_for_enter();
    clear_console();
}

void display_menu(void) {
    int choice;

    // storing the menu operations using function pointers
    void (*menu_operations[])(void) = {
        add_transaction,
        display_transactions,
        update_transaction,
        delete_transaction,
        filter_transactions,
        search_transactions,
        sort_transactions,
        generate_sales_report,
        display_products,
        add_product
    };

    while (true) {
        printf("\n\t===Sales App for Helen Stores===");

        printf("\n\t1. Add New Transaction");
        printf("\n\t2. Display All Transactions");
        printf("\n\t3. Update Transaction");
        printf("\n\t4. Delete Transaction");
        printf("\n\t5. Filter Transactions");
        printf("\n\t6. Search Transactions");
        printf("\n\t7. Sort Transactions");
        printf("\n\t8. Generate Sales Report");
        printf("\n\t9. Display products");
        printf("\n\t10. Add a new product");

        printf("\n\t0. Exit");

        if (!read_int(CYAN"\n\n\tSelect an action: "RESET, &choice)) {
            break;
        }

        // checking if the selected menu option exists
        if (choice < 0 || choice > 10) {
            display_error("Select an option between 0 and 10.");
            usleep(500000);
            continue;
        }

        // ending the program when zero is selected
        if (choice == 0) {
            printf("\nClosing the sales application.\n");
            sleep(1);
            break;
        }

        // calling the selected function through its pointer
        menu_operations[choice - 1]();
    }
}

int main(void) {
    // loading transactions before displaying the menu
    if (!load_transactions()) {
        free(transactions);
        return (1);
    }
    display_menu();

    free(transactions);

    return 0;
}