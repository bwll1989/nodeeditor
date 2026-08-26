set(CALC_SOURCE_FILES
    main.cpp
    MathOperationDataModel.cpp
    NumberDisplayDataModel.cpp
    NumberSourceDataModel.cpp
    InDataModel.cpp
    OutDataModel.cpp
    ContainerDataModel.cpp
    ViewsTabWidget.cpp
)

set(CALC_HEADER_FILES
    AdditionModel.hpp
    DivisionModel.hpp
    DecimalData.hpp
    MathOperationDataModel.hpp
    NumberDisplayDataModel.hpp
    NumberSourceDataModel.hpp
    SubtractionModel.hpp
    InDataModel.hpp
    OutDataModel.hpp
    ContainerDataModel.hpp
    ViewsTabWidget.hpp
)

add_executable(MultipleCalculator
  ${CALC_SOURCE_FILES}
  ${CALC_HEADER_FILES}
)

target_link_libraries(MultipleCalculator QtNodes)
