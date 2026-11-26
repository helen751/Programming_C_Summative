#!/bin/bash

# storing the location of the application files
SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="$SCRIPT_DIRECTORY/activity.log"

# defining colours for console messages
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
CYAN="\033[36m"
RESET="\033[0m"

# clearing the terminal
clear_console() {
    clear
}

# displaying an error message
display_error() {
    printf "${RED}\nError: %s${RESET}\n" "$1"
}

# displaying a warning message
display_warning() {
    printf "${YELLOW}\nWarning: %s${RESET}\n" "$1"
}

# displaying a success message
display_success() {
    printf "${GREEN}\n%s${RESET}\n" "$1"
}

# waiting before returning to the menu
wait_for_enter() {
    printf "${YELLOW}\nPress Enter to return to the menu...${RESET}"
    read -r
}

# checking if a command is available
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# recording an activity in the log file
write_log() {
    local message="$1"
    printf "[%s] %s\n" "$(date "+%d/%m/%Y %H:%M:%S")" "$message" >> "$LOG_FILE"
}

# reading and validating a whole number
read_integer() {
    local prompt="$1"
    local value

    while true; do
        printf "%b" "$prompt"
        read -r value

        if [[ "$value" =~ ^[0-9]+$ ]]; then
            REPLY="$value"
            return 0
        fi

        display_error "Enter a valid whole number."
    done
}

# reading and validating a percentage
read_percentage() {
    local prompt="$1"
    local value

    while true; do
        printf "%b" "$prompt"
        read -r value

        if [[ "$value" =~ ^[0-9]+([.][0-9]+)?$ ]] &&
            awk -v number="$value" \
            'BEGIN { exit !(number >= 0 && number <= 100) }'; then
            REPLY="$value"
            return 0
        fi

        display_error "Enter a percentage between 0 and 100."
    done
}

# reading and validating a yes or no response
read_confirmation() {
    local prompt="$1"
    local answer

    while true; do
        printf "%b" "$prompt"
        read -r answer

        case "$answer" in
            y|Y|yes|Yes|YES)
                REPLY="yes"
                return 0
                ;;
            n|N|no|No|NO)
                REPLY="no"
                return 0
                ;;
            *)
                display_error "Enter yes/y or no/n."
                ;;
        esac
    done
}

# getting the current CPU utilization
get_cpu_usage() {
    local cpu_usage
    local operating_system

    operating_system="$(uname -s)"

    if [[ "$operating_system" == "Darwin" ]]; then
        cpu_usage="$(
            top -l 1 2>/dev/null |
            awk '
                /CPU usage:/ {
                    idle = $(NF - 1)
                    gsub(/%/, "", idle)
                    print 100 - idle
                    exit
                }
            '
        )"
    else
        cpu_usage="$(
            LC_ALL=C top -bn1 2>/dev/null |
            awk '
                /Cpu\(s\)/ {
                    for (i = 1; i <= NF; i++) {
                        if ($i ~ /^id/) {
                            idle = $(i - 1)
                            gsub(/,/, "", idle)
                            print 100 - idle
                            exit
                        }
                    }
                }
            '
        )"
    fi

    if [[ -z "$cpu_usage" ]]; then
        return 1
    fi

    printf "%.2f" "$cpu_usage"
}

# displaying processes ordered by CPU utilization
show_top_processes() {
    local process_limit="$1"

    if ! command_exists ps; then
        display_error "The ps command is unavailable."
        return
    fi

    if [[ "$(uname -s)" == "Darwin" ]]; then
        ps -Ao pid,user,comm,%cpu,%mem,etime 2>/dev/null |
        head -1

        ps -Ao pid,user,comm,%cpu,%mem,etime 2>/dev/null |
        tail -n +2 |
        sort -k4 -nr |
        head -n "$process_limit"
    else
        ps -eo pid,user,comm,%cpu,%mem,etime \
        --sort=-%cpu 2>/dev/null |
        head -n "$((process_limit + 1))"
    fi
}

# displaying the basic system information
display_system_information() {
    local operating_system
    local uptime_information

    clear_console

    printf "${CYAN}\n\t===SYSTEM INFORMATION===${RESET}\n\n"

    if [[ -f /etc/os-release ]]; then
        operating_system="$(
            awk -F= '
                /^PRETTY_NAME=/ {
                    gsub(/"/, "", $2)
                    print $2
                }
            ' /etc/os-release
        )"
    else
        operating_system="$(uname -s)"
    fi

    if command_exists uptime; then
        uptime_information="$(uptime -p 2>/dev/null)"

        if [[ -z "$uptime_information" ]]; then
            uptime_information="$(uptime)"
        fi
    else
        uptime_information="Unavailable"
    fi

    printf "\t%-20s %s\n" "Hostname:" "$(hostname)"
    printf "\t%-20s %s\n" "Operating System:" "$operating_system"
    printf "\t%-20s %s\n" "Kernel Version:" "$(uname -r)"
    printf "\t%-20s %s\n" "System Uptime:" "$uptime_information"
    printf "\t%-20s %s\n" "Current Date:" "$(date "+%d/%m/%Y")"
    printf "\t%-20s %s\n" "Current Time:" "$(date "+%H:%M:%S")"

    write_log "Displayed system information."
    wait_for_enter
}

# monitoring the current CPU utilization
monitor_cpu() {
    local cpu_usage
    local threshold

    clear_console

    printf "${CYAN}\n\t===CPU UTILIZATION===${RESET}\n\n"

    cpu_usage="$(get_cpu_usage)"

    if [[ -z "$cpu_usage" ]]; then
        display_error "CPU information is unavailable."
        write_log "CPU monitoring failed."
        wait_for_enter
        return
    fi

    printf "\tCurrent CPU Utilization: %.2f%%\n" "$cpu_usage"

    read_percentage "\n\tEnter CPU warning threshold: "
    threshold="$REPLY"

    if awk -v usage="$cpu_usage" -v limit="$threshold" \
        'BEGIN { exit !(usage > limit) }'; then
        display_warning "CPU utilization is above ${threshold}%."

        write_log \
            "CPU warning: usage was ${cpu_usage}% and threshold was ${threshold}%."
    else
        display_success "CPU utilization is within the selected threshold."

        write_log \
            "CPU usage checked: ${cpu_usage}% with a ${threshold}% threshold."
    fi

    printf "${CYAN}\n\tProcesses Using the Most CPU${RESET}\n\n"

    show_top_processes 10

    wait_for_enter
}

# getting memory information for Linux or macOS
get_memory_usage() {
    local total_bytes
    local available_bytes
    local page_size
    local free_pages
    local inactive_pages
    local speculative_pages

    if [[ "$(uname -s)" == "Darwin" ]]; then
        total_bytes="$(sysctl -n hw.memsize 2>/dev/null)"

        if [[ ! "$total_bytes" =~ ^[0-9]+$ ]]; then
            return 1
        fi

        read -r page_size free_pages inactive_pages speculative_pages < <(
            vm_stat 2>/dev/null |
            awk '
                NR == 1 {
                    pageSize = $8
                }

                /^Pages free:/ {
                    value = $3
                    gsub(/\./, "", value)
                    freePages = value
                }

                /^Pages inactive:/ {
                    value = $3
                    gsub(/\./, "", value)
                    inactivePages = value
                }

                /^Pages speculative:/ {
                    value = $3
                    gsub(/\./, "", value)
                    speculativePages = value
                }

                END {
                    printf "%d %d %d %d\n",
                        pageSize,
                        freePages,
                        inactivePages,
                        speculativePages
                }
            '
        )

        if [[ ! "$page_size" =~ ^[0-9]+$ ]] ||
            [[ "$page_size" -eq 0 ]]; then
            return 1
        fi

        available_bytes="$(
            awk \
            -v size="$page_size" \
            -v free="$free_pages" \
            -v inactive="$inactive_pages" \
            -v speculative="$speculative_pages" \
            'BEGIN {
                print size * (free + inactive + speculative)
            }'
        )"

        awk \
        -v total="$total_bytes" \
        -v available="$available_bytes" \
        'BEGIN {
            totalMB = total / 1024 / 1024
            availableMB = available / 1024 / 1024
            usedMB = totalMB - availableMB
            percentage = (usedMB / totalMB) * 100

            printf "%.0f %.0f %.0f %.2f\n",
                totalMB,
                usedMB,
                availableMB,
                percentage
        }'
    elif command_exists free; then
        free -m |
        awk '
            /^Mem:/ {
                percentage = ($3 / $2) * 100
                printf "%d %d %d %.2f\n",
                    $2,
                    $3,
                    $7,
                    percentage
            }
        '
    else
        return 1
    fi
}

# monitoring the current memory utilization
monitor_memory() {
    local total_memory
    local used_memory
    local available_memory
    local memory_percentage
    local threshold

    clear_console

    printf "${CYAN}\n\t======== MEMORY UTILIZATION ========${RESET}\n\n"

    read -r total_memory used_memory available_memory memory_percentage < <(
        get_memory_usage
    )

    if [[ -z "$total_memory" ]]; then
        display_error "Memory information is unavailable."
        write_log "Memory monitoring failed."
        wait_for_enter
        return
    fi

    printf "\t%-23s %s MB\n" "Total Memory:" "$total_memory"
    printf "\t%-23s %s MB\n" "Used Memory:" "$used_memory"
    printf "\t%-23s %s MB\n" "Available Memory:" "$available_memory"
    printf "\t%-23s %.2f%%\n" "Memory Utilization:" "$memory_percentage"

    read_percentage "\n\tEnter memory warning threshold: "
    threshold="$REPLY"

    if awk -v usage="$memory_percentage" -v limit="$threshold" \
        'BEGIN { exit !(usage > limit) }'; then
        display_warning "Memory utilization is above ${threshold}%."

        write_log \
            "Memory warning: usage was ${memory_percentage}% and threshold was ${threshold}%."
    else
        display_success "Memory utilization is within the selected threshold."

        write_log \
            "Memory usage checked: ${memory_percentage}% with a ${threshold}% threshold."
    fi

    wait_for_enter
}

# getting disk information for Linux or macOS
get_disk_usage() {
    if ! command_exists df; then
        return 1
    fi

    if [[ "$(uname -s)" == "Darwin" ]]; then
        df -Ph 2>/dev/null
    else
        df -Ph -x tmpfs -x devtmpfs 2>/dev/null
    fi
}

# monitoring disk utilization
monitor_disk() {
    local threshold
    local filesystem
    local size
    local used
    local available
    local percentage
    local mount_point
    local numeric_percentage
    local warning_found=0
    local warning_filesystems=""

    clear_console

    printf "${CYAN}\n\t===DISK UTILIZATION===${RESET}\n"

    if ! command_exists df; then
        display_error "The df command is unavailable."
        write_log "Disk monitoring failed."
        wait_for_enter
        return
    fi

    read_percentage "\n\tEnter disk warning threshold: "
    threshold="$REPLY"

    printf "\n%-20s %-9s %-9s %-11s %-7s %s\n" \
        "Filesystem" "Size" "Used" "Available" "Use%" "Mounted On"

    printf "%-20s %-9s %-9s %-11s %-7s %s\n" \
        "------------------" \
        "-------" \
        "-------" \
        "---------" \
        "-----" \
        "----------------"

    while read -r filesystem size used available percentage mount_point; do
        printf "%-20s %-9s %-9s %-11s %-7s %s\n" \
            "$filesystem" \
            "$size" \
            "$used" \
            "$available" \
            "$percentage" \
            "$mount_point"

        numeric_percentage="${percentage%\%}"

        if awk -v usage="$numeric_percentage" -v limit="$threshold" \
            'BEGIN { exit !(usage > limit) }'; then
            warning_found=1
            warning_filesystems="${warning_filesystems}\n\t${filesystem} mounted on ${mount_point}: ${numeric_percentage}% used"

            write_log \
                "Disk warning: $filesystem was ${numeric_percentage}% full."
        fi
    done < <(
        get_disk_usage |
        tail -n +2
    )

    if [[ "$warning_found" -eq 1 ]]; then
        display_warning "The following filesystems exceeded the threshold:"
        printf "${YELLOW}%b${RESET}\n" "$warning_filesystems"
    else
        display_success "All filesystems are within the selected threshold."

        write_log \
            "Disk utilization checked with a ${threshold}% threshold."
    fi

    wait_for_enter
}

# displaying the currently running processes
display_running_processes() {
    clear_console

    printf "${CYAN}\n\t===RUNNING PROCESSES===${RESET}\n\n"

    if ! command_exists ps; then
        display_error "The ps command is unavailable."
        wait_for_enter
        return
    fi

    show_top_processes 30

    printf "\nShowing the first 30 processes ordered by CPU usage.\n"

    write_log "Displayed running processes."
    wait_for_enter
}

# searching for a process using its name or PID
search_process() {
    local search_value
    local results
    local matching_pids
    local matching_count
    local process_id
    local process_name
    local selected_value
    local answer
    local process_details
    local terminable_pids=""
    local terminated_count=0
    local skipped_count=0

    clear_console

    printf "${CYAN}\n\t===SEARCH PROCESSES===${RESET}\n"

    printf "\n\tEnter a process name or PID: "
    read -r search_value

    if [[ -z "$search_value" ]]; then
        display_error "The search value cannot be empty."
        wait_for_enter
        return
    fi

    if [[ "$search_value" =~ ^[0-9]+$ ]]; then
        results="$(
            ps -p "$search_value" \
            -o pid,user,comm,%cpu,%mem,etime 2>/dev/null
        )"
    else
        results="$(
            ps -eo pid,user,comm,%cpu,%mem,etime 2>/dev/null |
            awk -v search="$search_value" '
                NR == 1 ||
                index(tolower($0), tolower(search)) > 0 {
                    print
                }
            '
        )"
    fi

    if [[ -z "$results" ]] ||
        [[ "$(printf "%s\n" "$results" | wc -l)" -le 1 ]]; then
        display_warning "No matching process was found."
        write_log "No process was found for: $search_value."
        wait_for_enter
        return
    fi

    printf "\n%s\n" "$results"
    write_log "Searched for process: $search_value."

    matching_pids="$(printf "%s\n" "$results" | awk 'NR > 1 { print $1 }')"
    matching_count="$(printf "%s\n" "$matching_pids" | awk 'NF { count++ } END { print count + 0 }')"

    if [[ "$matching_count" -eq 1 ]]; then
        process_id="$matching_pids"
        process_name="$(ps -p "$process_id" -o comm= 2>/dev/null | xargs)"

        read_confirmation \
            "\n\tWould you like to terminate $process_name with PID $process_id? (yes/y or no/n): "
        answer="$REPLY"

        if [[ "$answer" == "no" ]]; then
            wait_for_enter
            return
        fi
    else
        read_confirmation \
            "\n\tWould you like to terminate any of these processes? (yes/y or no/n): "
        answer="$REPLY"

        if [[ "$answer" == "no" ]]; then
            wait_for_enter
            return
        fi

        printf "\n\tEnter one PID from the results or type all: "
        read -r selected_value

        case "$selected_value" in
            all|All|ALL)
                for process_id in $matching_pids; do
                    if ps -p "$process_id" -o pid= 2>/dev/null | grep -q '[0-9]' &&
                        ! is_protected_process "$process_id"; then
                        terminable_pids="$terminable_pids $process_id"
                    else
                        skipped_count=$((skipped_count + 1))
                    fi
                done

                terminable_pids="$(printf "%s" "$terminable_pids" | xargs)"

                if [[ -z "$terminable_pids" ]]; then
                    display_error "None of the matching processes can be terminated."
                    write_log "All matching processes were protected or unavailable."
                    wait_for_enter
                    return
                fi

                display_warning \
                    "This will immediately stop all allowed matching processes. Unsaved work may be lost."
                printf "\n\tProcesses that can be terminated: %s\n" "$terminable_pids"

                if [[ "$skipped_count" -gt 0 ]]; then
                    printf "\tProtected or unavailable processes skipped: %d\n" "$skipped_count"
                fi

                read_confirmation \
                    "\n\tConfirm termination of all allowed matching processes? (yes/y or no/n): "
                answer="$REPLY"

                if [[ "$answer" == "no" ]]; then
                    display_warning "Process termination was cancelled."
                    write_log "Termination of all searched processes was cancelled."
                    wait_for_enter
                    return
                fi

                for process_id in $terminable_pids; do
                    if kill "$process_id" 2>/dev/null; then
                        display_success "Process $process_id was terminated."
                        write_log "Terminated process with PID $process_id after a process search."
                        terminated_count=$((terminated_count + 1))
                    else
                        display_error "Process $process_id could not be terminated."
                        write_log "Failed to terminate PID $process_id."
                    fi
                done

                printf "\nSuccessfully terminated processes: %d\n" "$terminated_count"
                wait_for_enter
                return
                ;;
            *)
                if [[ ! "$selected_value" =~ ^[0-9]+$ ]]; then
                    display_error "Enter a displayed PID or type all."
                    wait_for_enter
                    return
                fi

                process_id="$selected_value"
                ;;
        esac

        if ! printf "%s\n" "$matching_pids" | grep -qx "$process_id"; then
            display_error "The PID was not included in the search results."
            write_log "Termination was rejected because PID $process_id was not in the search results."
            wait_for_enter
            return
        fi
    fi

    if ! ps -p "$process_id" -o pid= 2>/dev/null | grep -q '[0-9]'; then
        display_error "The selected process no longer exists."
        write_log "Failed termination attempt for PID $process_id."
        wait_for_enter
        return
    fi

    if is_protected_process "$process_id"; then
        display_error "This process is protected and cannot be terminated."
        write_log "Blocked termination attempt for PID $process_id."
        wait_for_enter
        return
    fi

    process_details="$(
        ps -p "$process_id" \
        -o pid,user,comm,%cpu,%mem,etime 2>/dev/null
    )"

    printf "\n%s\n" "$process_details"
    display_warning \
        "Terminating this process will stop it immediately and unsaved work may be lost."

    read_confirmation \
        "\n\tConfirm termination of PID $process_id? (yes/y or no/n): "
    answer="$REPLY"

    if [[ "$answer" == "no" ]]; then
        display_warning "Process termination was cancelled."
        write_log "Termination cancelled for PID $process_id."
        wait_for_enter
        return
    fi

    if kill "$process_id" 2>/dev/null; then
        display_success "Process $process_id was terminated."
        write_log "Terminated process with PID $process_id after a process search."
    else
        display_error "The process could not be terminated."
        write_log "Failed to terminate PID $process_id."
    fi

    wait_for_enter
}

# displaying the process submenu
view_and_search_processes() {
    local choice

    while true; do
        clear_console

        printf "${CYAN}\n\t===PROCESS VIEWER===${RESET}"

        printf "\n\n\t1. Display Running Processes"
        printf "\n\t2. Search for a Process"
        printf "\n\t0. Return to Main Menu"

        read_integer "\n\n\tSelect an action: "
        choice="$REPLY"

        if [[ "$choice" -eq 0 ]]; then
            return
        fi

        if [[ "$choice" -eq 1 ]]; then
            display_running_processes
        elif [[ "$choice" -eq 2 ]]; then
            search_process
        else
            display_error "Select an option between 0 and 2."
            sleep 1
        fi
    done
}

# checking if a process is protected
is_protected_process() {
    local process_id="$1"
    local process_name
    local process_owner
    local current_user

    if [[ "$process_id" -le 2 ]] ||
        [[ "$process_id" -eq "$$" ]] ||
        [[ "$process_id" -eq "$PPID" ]]; then
        return 0
    fi

    process_name="$(
        ps -p "$process_id" -o comm= 2>/dev/null |
        xargs
    )"

    process_owner="$(
        ps -p "$process_id" -o user= 2>/dev/null |
        xargs
    )"

    current_user="$(id -un)"

    case "$process_name" in
        systemd|init|kthreadd|ksoftirqd*|migration*|watchdog*)
            return 0
            ;;
    esac

    if [[ "$process_owner" != "$current_user" ]] &&
        [[ "$(id -u)" -ne 0 ]]; then
        return 0
    fi

    return 1
}

# terminating a process after confirmation
terminate_process() {
    local process_id
    local process_details
    local confirmation

    clear_console

    printf "${CYAN}\n\t===TERMINATE A PROCESS===${RESET}\n"

    read_integer "\n\tEnter the PID to terminate: "
    process_id="$REPLY"

    if ! ps -p "$process_id" -o pid= 2>/dev/null | grep -q '[0-9]'; then
        display_error "The selected process does not exist."
        write_log "Failed termination attempt for PID $process_id."
        wait_for_enter
        return
    fi

    if is_protected_process "$process_id"; then
        display_error "This process is protected and cannot be terminated."
        write_log "Blocked termination attempt for PID $process_id."
        wait_for_enter
        return
    fi

    process_details="$(
        ps -p "$process_id" \
        -o pid,user,comm,%cpu,%mem,etime 2>/dev/null
    )"

    printf "\n%s\n" "$process_details"
    display_warning \
        "Terminating this process will stop it immediately and unsaved work may be lost."

    read_confirmation \
        "\n\tConfirm termination of PID $process_id? (yes/y or no/n): "
    confirmation="$REPLY"

    if [[ "$confirmation" == "no" ]]; then
        display_warning "Process termination was cancelled."
        write_log "Termination cancelled for PID $process_id."
        wait_for_enter
        return
    fi

    if kill "$process_id" 2>/dev/null; then
        display_success "Process $process_id was terminated."
        write_log "Terminated process with PID $process_id."
    else
        display_error "The process could not be terminated."
        write_log "Failed to terminate PID $process_id."
    fi

    wait_for_enter
}

# generating a complete system health report
generate_health_report() {
    local report_directory
    local report_file
    local timestamp
    local cpu_usage
    local memory_information
    local confirmation
    local total_memory
    local used_memory
    local available_memory
    local memory_percentage

    clear_console

    printf "${CYAN}\n\t===GENERATE HEALTH REPORT===${RESET}\n"

    printf "\n\tEnter the directory for saving the report: "
    read -r report_directory

    if [[ -z "$report_directory" ]]; then
        display_error "The directory cannot be empty."
        wait_for_enter
        return
    fi

    if [[ "$report_directory" == "~" ]]; then
        report_directory="$HOME"
    elif [[ "$report_directory" == ~/* ]]; then
        report_directory="$HOME/${report_directory#~/}"
    fi

    if [[ ! -d "$report_directory" ]]; then
        read_confirmation \
            "\n\tThe directory does not exist. Create it? (yes/y or no/n): "
        confirmation="$REPLY"

        if [[ "$confirmation" == "yes" ]]; then
            if ! mkdir -p "$report_directory" 2>/dev/null; then
                display_error "The directory could not be created."
                write_log "Failed to create the report directory."
                wait_for_enter
                return
            fi
        else
            display_warning "Report generation was cancelled."
            wait_for_enter
            return
        fi
    fi

    if [[ ! -w "$report_directory" ]]; then
        display_error "The selected directory is not writable."
        write_log "Report directory was inaccessible."
        wait_for_enter
        return
    fi

    timestamp="$(date "+%Y%m%d_%H%M%S")"
    report_file="$report_directory/system_health_$timestamp.txt"
    cpu_usage="$(get_cpu_usage)"

    if [[ -z "$cpu_usage" ]]; then
        cpu_usage="Unavailable"
    else
        cpu_usage="${cpu_usage}%"
    fi

    read -r total_memory used_memory available_memory memory_percentage < <(
        get_memory_usage
    )

    if [[ -n "$total_memory" ]]; then
        memory_information="Total Memory: ${total_memory} MB
Used Memory: ${used_memory} MB
Available Memory: ${available_memory} MB
Memory Utilization: ${memory_percentage}%"
    else
        memory_information="Memory information is unavailable."
    fi

    {
        printf "============================================\n"
        printf "          SYSTEM HEALTH REPORT\n"
        printf "============================================\n\n"
        printf "Generated: %s\n" "$(date "+%d/%m/%Y %H:%M:%S")"
        printf "Hostname: %s\n" "$(hostname)"
        printf "Operating System: %s\n" "$(uname -s)"
        printf "Kernel Version: %s\n" "$(uname -r)"
        printf "System Uptime: %s\n" \
            "$(uptime -p 2>/dev/null || uptime)"

        printf "\nCPU UTILIZATION\n"
        printf '%s\n' "--------------------------------------------"
        printf "Current CPU Usage: %s\n" "$cpu_usage"

        printf "\nPROCESSES USING THE MOST CPU\n"
        printf '%s\n' "--------------------------------------------"

        show_top_processes 10

        printf "\nMEMORY UTILIZATION\n"
        printf '%s\n' "--------------------------------------------"
        printf "%b\n" "$memory_information"

        printf "\nDISK UTILIZATION\n"
        printf '%s\n' "--------------------------------------------"

        if command_exists df; then
            get_disk_usage
        else
            printf "Disk information is unavailable.\n"
        fi

        printf "\n============================================\n"
        printf "                END OF REPORT\n"
        printf "============================================\n"
    } > "$report_file"

    if [[ -s "$report_file" ]]; then
        display_success "System health report generated successfully."
        printf "\nSaved to: %s\n" "$report_file"

        write_log "Generated system health report: $report_file."
    else
        display_error "The report could not be generated."
        write_log "System health report generation failed."
    fi

    wait_for_enter
}

# displaying the saved activity history
view_activity_history() {
    clear_console

    printf "${CYAN}\n\t===ACTIVITY HISTORY===${RESET}\n\n"

    if [[ ! -s "$LOG_FILE" ]]; then
        display_warning "No activity has been recorded yet."
        wait_for_enter
        return
    fi

    cat "$LOG_FILE"
    wait_for_enter
}

# clearing the activity history
clear_activity_log() {
    local confirmation

    clear_console

    printf "${CYAN}\n\t===CLEAR ACTIVITY LOG===${RESET}\n"

    if [[ ! -s "$LOG_FILE" ]]; then
        display_warning "The activity log is already empty."
        wait_for_enter
        return
    fi

    read_confirmation \
        "\n\tClear the complete activity log? (yes/y or no/n): "
    confirmation="$REPLY"

    if [[ "$confirmation" == "yes" ]]; then
        : > "$LOG_FILE"

        display_success "The activity log was cleared."
        write_log "Activity log was cleared."
    else
        display_warning "Clearing the activity log was cancelled."
    fi

    wait_for_enter
}

# closing the application
exit_program() {
    clear_console

    write_log "Exited the system health monitoring application."

    printf "${GREEN}\nClosing the system health monitoring application...\n${RESET}"

    sleep 1
    exit 0
}

# displaying the main application menu
display_menu() {
    local choice

    # storing the menu operations
    local menu_operations=(
        display_system_information
        monitor_cpu
        monitor_memory
        monitor_disk
        view_and_search_processes
        terminate_process
        generate_health_report
        view_activity_history
        clear_activity_log
        exit_program
    )

    while true; do
        clear_console

        printf "${CYAN}\n\t===LINUX SYSTEM HEALTH MONITOR===${RESET}"

        printf "\n\n\t1. Display System Information"
        printf "\n\t2. Monitor CPU Utilization"
        printf "\n\t3. Monitor Memory Utilization"
        printf "\n\t4. Monitor Disk Utilization"
        printf "\n\t5. View and Search Running Processes"
        printf "\n\t6. Terminate a Process"
        printf "\n\t7. Generate System Health Report"
        printf "\n\t8. View Activity History"
        printf "\n\t9. Clear Activity Log"
        printf "\n\t0. Exit"

        read_integer "\n\n\tSelect an action: "
        choice="$REPLY"

        # checking if the selected option exists
        if [[ "$choice" -lt 0 || "$choice" -gt 9 ]]; then
            display_error "Select an option between 0 and 9."
            sleep 1
            continue
        fi

        # closing the program when zero is selected
        if [[ "$choice" -eq 0 ]]; then
            choice=10
        fi

        # calling the selected menu function
        "${menu_operations[$((choice - 1))]}"
    done
}

# creating the activity log file
touch "$LOG_FILE" 2>/dev/null

if [[ ! -w "$LOG_FILE" ]]; then
    display_error "The activity log cannot be created or updated."
    exit 1
fi

write_log "Started the system health monitoring application."

# starting the application
display_menu
