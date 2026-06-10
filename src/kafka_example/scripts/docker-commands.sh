#!/bin/bash

# Управление кластером
start_cluster() {
    echo "Starting Kafka cluster..."
    docker-compose up -d
    echo "Waiting for Kafka to be ready..."
    sleep 10
}

stop_cluster() {
    echo "Stopping Kafka cluster..."
    docker-compose down
}

restart_cluster() {
    echo "Restarting Kafka cluster..."
    docker-compose restart
}

# Мониторинг
show_cluster_status() {
    echo "=== Cluster Status ==="
    docker-compose ps
    
    echo -e "\n=== Broker Health ==="
    for i in 1 2 3; do
        echo -n "Broker $i: "
        docker exec kafka-broker-$i kafka-broker-api-versions --bootstrap-server localhost:9092 2>/dev/null && echo "✓" || echo "✗"
    done
}

show_topic_details() {
    local topic=$1
    if [ -z "$topic" ]; then
        echo "Usage: show_topic_details <topic-name>"
        return 1
    fi
    
    echo "=== Details for topic: $topic ==="
    docker exec kafka-broker-1 kafka-topics \
        --bootstrap-server localhost:9092 \
        --describe \
        --topic "$topic"
}

# Просмотр сообщений
consume_messages() {
    local topic=$1
    local from_beginning=${2:-false}
    
    if [ -z "$topic" ]; then
        echo "Usage: consume_messages <topic-name> [--from-beginning]"
        return 1
    fi
    
    CMD="docker exec kafka-broker-1 kafka-console-consumer --bootstrap-server localhost:9092 --topic $topic"
    
    if [ "$from_beginning" = true ]; then
        CMD="$CMD --from-beginning"
    fi
    
    echo "Consuming messages from $topic..."
    $CMD
}

# Удаление топиков
delete_topic() {
    local topic=$1
    if [ -z "$topic" ]; then
        echo "Usage: delete_topic <topic-name>"
        return 1
    fi
    
    echo "Deleting topic: $topic"
    docker exec kafka-broker-1 kafka-topics \
        --bootstrap-server localhost:9092 \
        --delete \
        --topic "$topic"
}

# Очистка всех данных
clean_all() {
    echo "WARNING: This will delete all Kafka data and volumes!"
    read -p "Are you sure? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        docker-compose down -v
        echo "All data cleaned."
    else
        echo "Operation cancelled."
    fi
}

# Меню
show_menu() {
    echo "=== Kafka Cluster Management ==="
    echo "1. Start cluster"
    echo "2. Stop cluster"
    echo "3. Restart cluster"
    echo "4. Show cluster status"
    echo "5. Show topic details"
    echo "6. Consume messages from topic"
    echo "7. Delete topic"
    echo "8. Clean all data (destructive!)"
    echo "9. Exit"
    echo -n "Choose option: "
}

# Основной цикл (если скрипт запущен напрямую)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    while true; do
        show_menu
        read option
        case $option in
            1) start_cluster ;;
            2) stop_cluster ;;
            3) restart_cluster ;;
            4) show_cluster_status ;;
            5) 
                echo -n "Enter topic name: "
                read topic
                show_topic_details "$topic"
                ;;
            6)
                echo -n "Enter topic name: "
                read topic
                echo -n "Read from beginning? (y/N): "
                read from_beginning
                if [[ $from_beginning =~ ^[Yy]$ ]]; then
                    consume_messages "$topic" true
                else
                    consume_messages "$topic" false
                fi
                ;;
            7)
                echo -n "Enter topic name to delete: "
                read topic
                delete_topic "$topic"
                ;;
            8) clean_all ;;
            9) exit 0 ;;
            *) echo "Invalid option" ;;
        esac
        echo
    done
fi