/*
# PostgreSQL Database Modeler (pgModeler)
# Sub-project: Core library (libpgmodeler)
# Class: OperationList
# Description: Implements the operations to maintain a list of modifications made
#              by the user on database model objects. This class permits that user
#              undo / redo all the operations made.
#
# Creation date: 17/07/2006
#
# Copyright 2006-2012 - Raphael Araújo e Silva <rkhaotix@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The complete text of GPLv3 is at LICENSE file on source code root directory.
# Also, you can get the complete GNU General Public License at <http://www.gnu.org/licenses/>
*/
#ifndef OPERATIONLIST_H
#define OPERATIONLIST_H

#include "modelobd.h"
#include <QObject>

class Operation {
 protected:
 /* Reference to the parent object of the original object that has suffered the operation,
    This attribute is used only in the case of table objects such as columns, indexes,
    constraints, rules. For other object types there is no need to use this attribute
    because the parent object for other objects is always the database model */
  BaseObject *parent_obj;

  //Reference (on the pool) to the copy of the original object
  BaseObject *pool_obj;

  //Reference to the original object that generates the operation.
  BaseObject *original_obj;

  /* Stores the XML definition of the special objects this means the objects
     that reference columns added by relationship. This is the case of triggers,
     indexes, sequences, constraints. */
  QString xml_definition;

  //Operation type (Constants OBJECT_[MODIFIED | CREATED | REMOVED | MOVED]
  unsigned op_type;

  //Operation chain type. This attribute is used to redo/undo several operations at once
  unsigned chain_type;

  //Object index inside the list on its parent object
  int object_idx;

 public:
  //Constants used to reference the type of operations
  static const unsigned OBJECT_MODIFIED=0,
                        OBJECT_CREATED=1,
                        OBJECT_REMOVED=2,
                        /* This type of operation has the same effect of operation OBJETO_MODIFIED
                           except that it not (re)validate relationships as happens with operations.
                           This type of operation (OBJECT_MOVED) is useful to undo position changes of
                           graphical objects without executing unnecessary revalidations of relationships */
                        OBJECT_MOVED=3;

  //Operation chain types
  static const unsigned NO_CHAIN=10, //The operation is not part of a chain
                        CHAIN_START=11, //The operation is the head of the chain
                        CHAIN_MIDDLE=12, //The operation is in the middle of the chain
                        CHAIN_END=13; //The operation is the last on the chain


  Operation(void)
  { parent_obj=NULL; pool_obj=NULL; original_obj=NULL;
    object_idx=-1; chain_type=NO_CHAIN; }

 friend class OperationList;
};

class OperationList: public QObject {
 private:
  Q_OBJECT

  //Indica que o encadeamento de operações está anulado temporariamente
  bool ignore_chain;

  //Lista de objetos excluídos / modificado no modelo
  vector<BaseObject *> object_pool;

  /* Lista de objetos que no momento da exclusão do pool ainda eram referenciados
     de algum modo no modelo. Os mesmo são armazenados nesta lista secundária e
     excluídos quando a lista de operações objeto da lista
     em si é destruído */
  vector<BaseObject *> not_removed_objs;

  //Lista de operações executadas pelo usuário
  vector<Operation *> operations;

  //Modelo ao qual a lista se aplica
  ModeloBD *model;

  //Número máximo de entradas que a lista pode aceitar (global)
  static unsigned max_size;

           /* Armazena o tipo de encadeamento para a próxima
              operação a ser armazenada na lista. Este atributo
              é usado em conjunto com os metodos de inicialização
              e encerramento de encademanento de operações */
  unsigned next_op_chain;

  //Índice atual da lista de operações
  int current_index;

  /* Valida as operações verificando se as mesmas possuem objetos registrados no pool.
     Caso seja encontrada alguma operação cujo objeto não se encontra no pool
     ela será removida, pois um objeto fora do pool não dá a garantia de que está
     sendo referenciado no modelo. */
  void validateOperations(void);

  //Verifica se o objeto passado encontra-se no pool
  bool isObjectOnPool(BaseObject *object);

  //Adiciona o objeto no pool de acordo com o tipo da operação informado
  void addToPool(BaseObject *object, unsigned op_type);

  /* Remove um objeto do pool através de seu índice efetuando a desalocação caso
     nenhum objeto esteja referenciando o mesmo */
  void removeFromPool(unsigned obj_idx);

  //Executa uma operação da lista
  void executeOperation(Operation *operacao, bool redo);

  //Retorna o tamanho do encadeamento de operações a partir da posição atual
  unsigned getChainSize(void);

 public:
  OperationList(ModeloBD *model);
  ~OperationList(void);

  /* Inicia o encadeamento de operações.
     Isso significa que todas as operações adicionadas
     após a chamada deste método serão consideradas a
     serem executadas todas em cadeia com uma única chamada
     aos métodos refazerOperacao ou desfazerOperacao. */
  void startOperationChain(void);

  /* Finaliza o encadeamento das operações definindo que a
     ultima operação adicionada é o final da lista de
     encadeamento */
  void finishOperationChain(void);

  /* Anula a execução das operações em forma de encadeamento,
     porém caso a lista esteja com encadeamento aberto, as operações
     serão inseridas em cadeia. Este método ajuda em situações onde
     é necessário remover operações ou executá-las uma a uma porém
     mantendo-se o encadeamento criado anteriormente.

     Obs.: O usuário deve cancelar o anulamento do encademanto para
           conseguir finalizaer o encadeamento de operações, se isso não
           acontecer, as operações serão criadas encadeadas idefinidademente */
  void ignoreOperationChain(bool value);

  //Retorna se um encadeamento na lista já foi iniciado
  bool isOperationChainStarted(void);

  //Desfaz a última operação criada na lista
  void undoOperation(void);

  //Refaz a próxima operação criada na lista
  void redoOperation(void);

  //Remove todas as operações da lista
  void removeOperations(void);

  //Obtém os dados da operação no índice especificado
  void getOperationData(unsigned oper_idx, unsigned &oper_type, QString &obj_name, ObjectType &obj_type);

  //Define o tamanho máximo da lista
  static void setMaximumSize(unsigned max);

  /* Registra na lista de operaçõe que o objeto passado sofreu algum tipo
     de operação (modificação, removido, inserido) além de armazenar o conteúdo
     antigo do mesmo. Este método SEMPRE deve ser chamado antes de que o objeto em questão
     sofra qualquer operação no modelo. Caso este método seja chamado após uma operação sobre o
     objeto, a ordem de restauração/reexecução de operações pode ser quebrada
     ocasionando em segmentations fault. */
  void registerObject(BaseObject *object, unsigned op_type, int object_idx=-1, BaseObject *parent_obj=NULL);

  //Obtém o tamanho máximo da lista
  unsigned getMaximumSize(void);

  //Obtém o tamanho atual da lista
  unsigned getCurrentSize(void);

  //Obtém o indice atual da lista
  int getCurrentIndex(void);

  /* Retorna se a lista de operações está preparada para executar
     as operações de refazer e desfazer */
  bool isRedoAvailable(void);
  bool isUndoAvailable(void);

  /* Remove a última operação da lista. Este método deve ser usado com
     cuidado pois ele pode quebrar o encadeamento de operações. Deve ser
     usado somente quando uma exceção é disparada no momento após a adição
     de algum objeto a lista e se deseja descartar esse operação devido
     a exceção disparada. Caso a última operação for parte de um encadeamento,
     toda a cadeia de operações é removido. Atenção: O funcionamento dste método
     é diferente do método de desfazer operação, pois os objetos são removidos
     do pool porém seus estados anteriores a adição dos mesmos a  lista não são
     restaurados, por isso este não pode ser usado deliberadamente. */
  void removeLastOperation(void);

  /* Atualiza o índice do objeto quando o mesmo sofre uma movimentação no objeto
     pai. Geralmente este método não precisa ser chamado, mas no caso de objetos
     de tabela os quais podem ser movimentado (ter sua posição no objeto pai alterada)
     este método atualiza o índice do objeto com o novo valor para que as operações
     as quais referenciam tal objeto não seja executadas de forma incorreta usando
     o índice anterior */
  void updateObjectIndex(BaseObject *object, unsigned new_idx);

  signals:
   //Sinal disparado para cada operação encadeada que for executada
   void s_operationExecuted(int progress, QString object_id, unsigned icon_id);
};


/* Função modelo faz a cópia dos atributos do obj_copia para obj_orig fazendo
   o cast para o tipo de objeto correto. Caso o objeto de origem não esteja alocado
   a função o aloca antes de copiar os atributos. Os dois objetos devem ser do mesmo tipo
   caso ambos estejam alocados.
   -- Sintaxe no estilo  brainfuck! :p -- */
template <class Classe>
void copyObject(BaseObject **porig_obj, Classe *copy_obj);

/* Esta função apenas chama a função modelo acima fazendo o dynamic_cast correto,
   de acordo com o tipo passado */
void copyObject(BaseObject **porig_obj, BaseObject *copy_obj, ObjectType obj_type);

#endif