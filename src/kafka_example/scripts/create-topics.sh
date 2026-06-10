#!/bin/bash

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Creating Kafka Topics for Order Management System ===${NC}"

# Определяем replication factor в зависимости от количества брокеров
# Для одного брокера используем replication-factor=1
REPLICATION_FACTOR=1

# Функция для создания топика
create_topic() {
    local topic=$1
    local partitions=$2
    local replication=$3
    
    echo -e "${YELLOW}Creating topic: $topic (partitions: $partitions, replication: $replication)${NC}"
    
    sudo docker exec kafka-broker kafka-topics \
        --bootstrap-server localhost:9092 \
        --create \
        --topic "$topic" \
        --partitions "$partitions" \
        --replication-factor "$replication" \
        --if-not-exists
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Topic $topic created successfully${NC}"
    else
        echo -e "${RED}✗ Failed to create topic $topic${NC}"
    fi
}

# Создание основных топиков (с replication factor 1)
create_topic "order-confirmation" 6 $REPLICATION_FACTOR
create_topic "order-status-update" 6 $REPLICATION_FACTOR
create_topic "order-cancellation" 6 $REPLICATION_FACTOR
create_topic "customer-notification" 6 $REPLICATION_FACTOR
create_topic "audit-log" 12 $REPLICATION_FACTOR

# Dead Letter Queues
create_topic "order-confirmation-dlq" 3 $REPLICATION_FACTOR
create_topic "order-status-update-dlq" 3 $REPLICATION_FACTOR
create_topic "order-cancellation-dlq" 3 $REPLICATION_FACTOR

echo -e "\n${GREEN}=== All topics created successfully! ===${NC}"

# Показать все топики
echo -e "\n${YELLOW}Current topics:${NC}"
sudo docker exec kafka-broker kafka-topics --bootstrap-server localhost:9092 --list